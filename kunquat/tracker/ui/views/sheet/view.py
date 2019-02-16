# -*- coding: utf-8 -*-

#
# Authors: Tomi Jylhä-Ollila, Finland 2013-2018
#          Toni Ruottu, Finland 2014
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from itertools import count, islice
import math
import time

from kunquat.tracker.ui.qt import *

import kunquat.kunquat.events as events
from kunquat.kunquat.limits import *
import kunquat.tracker.cmdline as cmdline
import kunquat.tracker.ui.model.tstamp as tstamp
from kunquat.tracker.ui.model.keymapmanager import KeyboardAction
from kunquat.tracker.ui.model.sheetmanager import SheetManager
from kunquat.tracker.ui.model.trigger import Trigger
from kunquat.tracker.ui.model.triggerposition import TriggerPosition
from kunquat.tracker.ui.views.keyboardmapper import KeyboardMapper
from kunquat.tracker.ui.views.updater import Updater
from .config import *
from . import utils
from .columngrouprenderer import ColumnGroupRenderer
from .triggercache import TriggerCache
from .triggerrenderer import TriggerRenderer
from .movestate import HorizontalMoveState, VerticalMoveState


class TriggerTypeValidator(QValidator):

    def __init__(self):
        super().__init__()

    def validate(self, contents, pos):
        in_str = str(contents)
        parts = in_str.split(':')
        event_name = parts[0]
        if event_name in events.trigger_events_by_name:
            if len(parts) > 2:
                return (QValidator.Invalid, contents, pos)
            elif len(parts) == 2:
                name = parts[1]
                if ((not all(ch in DEVICE_EVENT_CHARS for ch in name)) or
                        (len(name) > DEVICE_EVENT_NAME_MAX)):
                    return (QValidator.Invalid, contents, pos)
                if (not name) or event_name not in events.trigger_events_with_name_spec:
                    return (QValidator.Intermediate, contents, pos)
            return (QValidator.Acceptable, contents, pos)
        else:
            return (QValidator.Intermediate, contents, pos)


class TriggerArgumentValidator(QValidator):

    def __init__(self):
        super().__init__()

    def validate(self, contents, pos):
        if not all(0x20 <= ord(c) <= 0x7e for c in contents):
            return (QValidator.Invalid, contents, pos)
        return (QValidator.Acceptable, contents, pos)


class FieldEdit(QLineEdit):

    def __init__(self, parent):
        super().__init__(parent)
        self.hide()

        self._finished_callback = None

        self.editingFinished.connect(self._finished)

    def _finished(self):
        assert self._finished_callback
        text = str(self.text())
        cb = self._finished_callback
        self._finished_callback = None
        cb(text)

    def start_editing(
            self,
            x_offset,
            y_offset,
            validator,
            finished_callback,
            start_input=''):
        self.move(x_offset - 2, y_offset - 2)
        self.setText(start_input)
        self.setSelection(0, len(start_input))
        assert not self._finished_callback
        self._finished_callback = finished_callback
        self.setValidator(validator)
        self.show()
        self.setFocus()

    def is_editing(self):
        return self.isVisible()

    def keyPressEvent(self, event):
        if super().keyPressEvent(event):
            return
        if event.key() == Qt.Key_Escape:
            self.parent().setFocus()
        event.accept()

    def focusOutEvent(self, event):
        self.hide()
        self._finished_callback = None


class View(QWidget, Updater):

    heightChanged = Signal(name='heightChanged')
    followCursor = Signal(str, int, name='followCursor')
    followPlaybackColumn = Signal(int, name='followPlaybackColumn')
    checkFixFocus = Signal(name='checkFixFocus')

    def __init__(self):
        self._force_focus_on_enable = True
        super().__init__()

        self._sheet_mgr = None
        self._notation_mgr = None
        self._visibility_mgr = None
        self._playback_mgr = None
        self._keyboard_mapper = KeyboardMapper()

        self.setAutoFillBackground(False)
        self.setAttribute(Qt.WA_OpaquePaintEvent)
        self.setAttribute(Qt.WA_NoSystemBackground)
        self.setFocusPolicy(Qt.StrongFocus)
        self.setMouseTracking(True)

        self._px_per_beat = None
        self._px_offset = 0
        self._pinsts = []

        self._col_width = None
        self._first_col = 0
        self._visible_cols = 0

        self._edit_first_col = 0
        self._edit_px_offset = 0

        self._trigger_cache = TriggerCache()
        self._col_rends = [
                ColumnGroupRenderer(i, self._trigger_cache) for i in range(COLUMNS_MAX)]

        self._prev_first_kept_col = 0
        self._prev_last_kept_col = COLUMNS_MAX - 1

        self._upcoming_test_start_index_add = count()

        self._heights = []
        self._start_heights = []

        self._horizontal_move_state = HorizontalMoveState()
        self._vertical_move_state = VerticalMoveState()

        self._trow_px_offset = 0

        self._mouse_selection_snapped_out = False

        self._field_edit = FieldEdit(self)

        self._is_playback_cursor_visible = False
        self._playback_cursor_offset = None
        self._was_playback_active = False

    def _on_setup(self):
        self._sheet_mgr = self._ui_model.get_sheet_manager()
        self._notation_mgr = self._ui_model.get_notation_manager()
        self._visibility_mgr = self._ui_model.get_visibility_manager()
        self._playback_mgr = self._ui_model.get_playback_manager()

        self._keyboard_mapper.set_ui_model(self._ui_model)
        for cr in self._col_rends:
            cr.set_ui_model(self._ui_model)

        self.register_action('signal_notation', self._update_notation)
        self.register_action('signal_hits', self._update_hit_names)
        self.register_action('signal_ch_defaults', self._update_hit_names)
        self.register_action('signal_module', self._update_all_patterns)
        self.register_action('signal_order_list', self._rearrange_patterns)
        self.register_action('signal_pattern_length', self._update_resized_patterns)
        self.register_action('signal_selection', self._follow_edit_cursor)
        self.register_action('signal_edit_mode', self.update)
        self.register_action('signal_replace_mode', self.update)
        self.register_action('signal_channel_mute', self.update)
        self.register_action('signal_grid', self._update_grid)
        self.register_action('signal_grid_pattern_modified', self._update_grid)
        self.register_action('signal_force_shift', self._update_force_shift)
        self.register_action('signal_silence', self._handle_silence)
        self.register_action('signal_playback_cursor', self._update_playback_cursor)
        self.register_action('signal_column_updated', self._update_columns)

        self.checkFixFocus.connect(self._check_fix_focus, Qt.QueuedConnection)

    def _update_columns(self):
        column_updates = self._sheet_mgr.get_and_clear_column_updates()
        for cu in column_updates:
            track_num, system_num, col_num = cu
            self._update_column(track_num, system_num, col_num)

            # Check if the pattern has multiple instances
            module = self._ui_model.get_module()
            album = module.get_album()
            assert album
            song = album.get_song_by_track(track_num)
            signal_pinst = song.get_pattern_instance(system_num)
            signal_pat_num = signal_pinst.get_pattern_num()
            signal_pinst_num = signal_pinst.get_instance_num()

            if len(signal_pinst.get_pattern().get_instance_ids()) > 1:
                # Update columns of other instances
                for cur_track_num in range(album.get_track_count()):
                    cur_song = album.get_song_by_track(cur_track_num)
                    for cur_system_num in range(cur_song.get_system_count()):
                        cur_pinst = song.get_pattern_instance(cur_system_num)
                        if ((cur_pinst.get_pattern_num() == signal_pat_num) and
                                (cur_pinst.get_instance_num() != signal_pinst_num)):
                            self._update_column(
                                    cur_track_num, cur_system_num, col_num)

            self.update()

    def _update_all_patterns(self):
        for cr in self._col_rends:
            cr.flush_caches()
        all_pinsts = utils.get_all_pattern_instances(self._ui_model)
        self.set_pattern_instances(all_pinsts)
        self._update_playback_cursor()
        self.update()

    def _update_playback_cursor(self):
        was_playback_cursor_visible = self._is_playback_cursor_visible

        prev_offset = self._playback_cursor_offset

        self._is_playback_cursor_visible = False
        self._playback_cursor_offset = None

        if self._playback_mgr.is_playback_active():
            track_num, system_num, row_ts = self._playback_mgr.get_playback_position()
            if track_num < 0 or system_num < 0:
                ploc = utils.get_current_playback_pattern_location(self._ui_model)
                if ploc:
                    track_num, system_num = ploc
            location = TriggerPosition(track_num, system_num, 0, row_ts, 0)
            self._playback_cursor_offset = self._get_row_offset(location, absolute=True)

        if self._playback_cursor_offset != None:
            if self._playback_mgr.follow_playback_cursor():
                self._is_playback_cursor_visible = True
                self._follow_playback_cursor()
            elif 0 <= (self._playback_cursor_offset - self._px_offset) < self.height():
                self._is_playback_cursor_visible = True
                if prev_offset != self._playback_cursor_offset:
                    self.update()

        if (not self._is_playback_cursor_visible) and was_playback_cursor_visible:
            self.update()

        self._was_playback_active = self._playback_mgr.is_playback_active()

    def _handle_silence(self):
        if self._was_playback_active:
            self._was_playback_active = False
            max_visible_cols = utils.get_max_visible_cols(self.width(), self._col_width)
            edit_location = self._ui_model.get_selection().get_location()
            edit_col_num = edit_location.get_col_num()
            edit_row_offset = self._get_row_offset(edit_location)

            if ((0 <= edit_row_offset < self.height()) and
                    (0 <= edit_col_num - self._first_col < max_visible_cols)):
                self._follow_edit_cursor()
            else:
                self.followCursor.emit(str(self._edit_px_offset), self._edit_first_col)

    def _rearrange_patterns(self):
        self._pinsts = utils.get_all_pattern_instances(self._ui_model)
        lengths = [pinst.get_pattern().get_length() for pinst in self._pinsts]
        for cr in self._col_rends:
            cr.rearrange_patterns(self._pinsts)
            cr.set_pattern_lengths(lengths)
        self._set_pattern_heights()
        self.update()

    def _update_grid(self):
        for cr in self._col_rends:
            cr.flush_final_pixmaps()
        self.update()

    def _update_notation(self):
        for cr in self._col_rends:
            cr.flush_caches()
        self.update()

    def _update_hit_names(self):
        for cr in self._col_rends[:self._first_col]:
            cr.flush_caches()

        max_visible_cols = utils.get_max_visible_cols(self.width(), self._col_width)
        vis_col_stop = min(COLUMNS_MAX, self._first_col + max_visible_cols)

        for cr in self._col_rends[self._first_col:vis_col_stop]:
            cr.update_hit_names(self.height())

        for cr in self._col_rends[vis_col_stop:]:
            cr.flush_caches()

        self.update()

    def _update_resized_patterns(self):
        self._rearrange_patterns()
        self._update_playback_cursor()
        self.update()

    def _update_force_shift(self):
        for cr in self._col_rends:
            cr.flush_caches()
        self.update()

    def _update_column(self, track_num, system_num, col_num):
        pattern_index = utils.get_pattern_index_at_location(
                self._ui_model, track_num, system_num)
        col_data = self._pinsts[pattern_index].get_column(col_num)
        self._col_rends[col_num].update_column(pattern_index, col_data)

    def set_config(self, config):
        self._config = config
        self._trigger_cache.flush()
        for cr in self._col_rends:
            cr.set_config(self._config)
        self.update()

    def set_first_column(self, first_col):
        if self._first_col != first_col:
            self._first_col = first_col
            if not self._playback_mgr.follow_playback_cursor():
                self._edit_first_col = first_col
            self.update()

    def set_pattern_instances(self, pinsts):
        self._pinsts = pinsts
        self._set_pattern_heights()
        lengths = [pinst.get_pattern().get_length() for pinst in pinsts]
        for i, cr in enumerate(self._col_rends):
            cr.set_pattern_lengths(lengths)
            columns = [pinst.get_column(i) for pinst in pinsts]
            cr.set_columns(columns)

    def _set_pattern_heights(self):
        lengths = [pinst.get_pattern().get_length() for pinst in self._pinsts]
        self._heights = utils.get_pat_heights(lengths, self._px_per_beat)
        self._start_heights = utils.get_pat_start_heights(self._heights)
        for cr in self._col_rends:
            cr.set_pattern_heights(self._heights, self._start_heights)

        self.heightChanged.emit()

    def get_total_height(self):
        return sum(self._heights)

    def set_px_offset(self, offset):
        if self._px_offset != offset:
            self._px_offset = offset
            for cr in self._col_rends:
                cr.set_px_offset(self._px_offset)
            if not self._playback_mgr.follow_playback_cursor():
                self._edit_px_offset = offset
            self.update()

    def set_px_per_beat(self, px_per_beat):
        if self._px_per_beat != px_per_beat:
            # Get old edit cursor offset
            location = TriggerPosition(0, 0, 0, tstamp.Tstamp(0), 0)
            if self._ui_model:
                selection = self._ui_model.get_selection()
                location = selection.get_location() or location
            orig_px_offset = self._px_offset
            orig_relative_offset = self._get_row_offset(location) or 0

            # Update internal state
            self._px_per_beat = px_per_beat
            for cr in self._col_rends:
                cr.set_px_per_beat(self._px_per_beat)
            self._set_pattern_heights()

            # Adjust vertical position so that edit cursor maintains its height
            new_cursor_offset = self._get_row_offset(location, absolute=True) or 0
            new_px_offset = new_cursor_offset - orig_relative_offset
            self.followCursor.emit(str(new_px_offset), self._first_col)

    def set_column_width(self, col_width):
        if self._col_width != col_width:
            self._col_width = col_width
            max_visible_cols = utils.get_max_visible_cols(self.width(), self._col_width)
            first_col = utils.clamp_start_col(self._first_col, max_visible_cols)
            visible_cols = utils.get_visible_cols(first_col, max_visible_cols)

            self._first_col = first_col
            self._visible_cols = visible_cols

            for cr in self._col_rends:
                cr.set_width(self._col_width)
            self.update()

            if self._playback_mgr.follow_playback_cursor():
                new_first_col = self._first_col
            else:
                # Adjust horizontal position so that edit cursor is visible
                location = TriggerPosition(0, 0, 0, tstamp.Tstamp(0), 0)
                if self._ui_model:
                    selection = self._ui_model.get_selection()
                    location = selection.get_location() or location
                edit_col_num = location.get_col_num()

                new_first_col = self._first_col
                x_offset = self._get_col_offset(edit_col_num)
                if x_offset < 0:
                    new_first_col = edit_col_num
                elif x_offset + self._col_width > self.width():
                    new_first_col = edit_col_num - (self.width() // self._col_width) + 1

            max_visible_cols = utils.get_max_visible_cols(self.width(), self._col_width)
            new_first_col = utils.clamp_start_col(new_first_col, max_visible_cols)

            self.followCursor.emit(str(self._px_offset), new_first_col)

    def _get_col_offset(self, col_num):
        max_visible_cols = utils.get_max_visible_cols(self.width(), self._col_width)
        first_col = utils.clamp_start_col(self._first_col, max_visible_cols)
        return (col_num - first_col) * self._col_width

    def _get_row_offset(self, location, absolute=False):
        ref_offset = 0 if absolute else self._px_offset

        # Get location components
        track = location.get_track()
        system = location.get_system()
        row_ts = location.get_row_ts()

        # Get pattern that contains our location
        try:
            module = self._ui_model.get_module()
            album = module.get_album()
            song = album.get_song_by_track(track)
            cur_pinst = song.get_pattern_instance(system)
        except (IndexError, AttributeError):
            return None

        for pinst, start_height in zip(self._pinsts, self._start_heights):
            if cur_pinst == pinst:
                start_px = start_height - ref_offset
                location_from_start_px = (
                        (row_ts.beats * tstamp.BEAT + row_ts.rem) *
                        self._px_per_beat) // tstamp.BEAT
                location_px = location_from_start_px + start_px
                return location_px

        return None

    def _get_init_trigger_row_width(self, rends, trigger_index):
        total_width = sum(islice(
                (r.get_total_width() for r in rends), 0, trigger_index))
        return total_width

    def _follow_trigger_row(self, location):
        module = self._ui_model.get_module()
        album = module.get_album()

        if album and album.get_track_count() > 0:
            cur_column = self._sheet_mgr.get_column_at_location(location)

            row_ts = location.get_row_ts()
            if row_ts in cur_column.get_trigger_row_positions():
                notation = self._notation_mgr.get_selected_notation()
                force_shift = module.get_force_shift()

                # Get trigger row width information
                trigger_index = location.get_trigger_index()
                trigger_count = cur_column.get_trigger_count_at_row(row_ts)
                triggers = [cur_column.get_trigger(row_ts, i)
                        for i in range(trigger_count)]
                rends = [TriggerRenderer(
                    self._trigger_cache, self._config, t, notation, force_shift)
                    for t in triggers]
                row_width = sum(r.get_total_width() for r in rends)

                init_trigger_row_width = self._get_init_trigger_row_width(
                        rends, trigger_index)

                trigger_padding = self._config['trigger']['padding_x']

                border_width = self._config['border_width']
                vis_width = self._col_width - border_width * 2

                # Upper bound for row offset
                hollow_rect = self._get_hollow_replace_cursor_rect()
                trail_width = hollow_rect.width() + trigger_padding
                tail_offset = max(0, row_width + trail_width - vis_width)

                max_offset = min(tail_offset, init_trigger_row_width)

                # Lower bound for row offset
                if trigger_index < len(triggers):
                    renderer = TriggerRenderer(
                            self._trigger_cache,
                            self._config,
                            triggers[trigger_index],
                            notation,
                            force_shift)
                    # TODO: revisit field bounds handling, this is messy
                    trigger_width = renderer.get_total_width()
                else:
                    trigger_width = trail_width
                min_offset = max(0,
                        init_trigger_row_width - vis_width + trigger_width)

                # Final offset
                self._trow_px_offset = min(max(
                    min_offset, self._trow_px_offset), max_offset)

    def _follow_playback_cursor(self):
        selection = self._ui_model.get_selection()
        selection_location = selection.get_location()
        col_num = self._first_col

        track_num, system_num, row_ts = self._playback_mgr.get_playback_position()
        if track_num < 0 or system_num < 0:
            ploc = utils.get_current_playback_pattern_location(self._ui_model)
            if ploc:
                track_num, system_num = ploc
            else:
                # Location could not be determined
                return

        location = TriggerPosition(track_num, system_num, col_num, row_ts, 0)
        self._follow_location(location, self.height() // 2)

    def _follow_edit_cursor(self):
        selection = self._ui_model.get_selection()
        location = selection.get_location()
        self._follow_location(location, self._config['edit_cursor']['min_snap_dist'])

    def _follow_location(self, location, min_snap_dist):
        if not location:
            self.update()
            return

        is_view_scrolling_required = False
        new_first_col = self._first_col
        new_y_offset = self._px_offset

        # Check column scrolling
        col_num = location.get_col_num()

        x_offset = self._get_col_offset(col_num)
        if x_offset < 0:
            is_view_scrolling_required = True
            new_first_col = col_num
        elif x_offset + self._col_width > self.width():
            is_view_scrolling_required = True
            new_first_col = col_num - (self.width() // self._col_width) + 1

        # Check scrolling inside a trigger row
        self._follow_trigger_row(location)

        # Check vertical scrolling
        y_offset = self._get_row_offset(location)
        if y_offset == None:
            self.update()
            return
        min_centre_dist = min(min_snap_dist, self.height() // 2)
        tr_height = self._config['tr_height']
        min_y_offset = min_centre_dist - tr_height // 2
        max_y_offset = self.height() - min_centre_dist - tr_height // 2
        if y_offset < min_y_offset:
            is_view_scrolling_required = True
            new_y_offset = new_y_offset - (min_y_offset - y_offset)
        elif y_offset >= max_y_offset:
            is_view_scrolling_required = True
            new_y_offset = new_y_offset + (y_offset - max_y_offset)

        if is_view_scrolling_required:
            self.followCursor.emit(str(new_y_offset), new_first_col)
        else:
            self.update()

    def _draw_edit_cursor(self, painter):
        if not self._pinsts:
            return

        selection = self._ui_model.get_selection()
        location = selection.get_location()

        selected_col = location.get_col_num()
        row_ts = location.get_row_ts()
        trigger_index = location.get_trigger_index()

        border_width = self._config['border_width']

        # Get pixel offsets
        x_offset = self._get_col_offset(selected_col)
        if not 0 <= x_offset < self.width():
            return
        y_offset = self._get_row_offset(location)
        if not -self._config['tr_height'] < y_offset < self.height():
            return

        # Draw guide extension line
        if self._sheet_mgr.is_editing_enabled():
            painter.setPen(self._config['edit_cursor']['guide_colour'])
            visible_col_nums = range(
                self._first_col,
                min(COLUMNS_MAX, self._first_col + self._visible_cols))
            for col_num in visible_col_nums:
                if col_num != selected_col:
                    col_x_offset = self._get_col_offset(col_num)
                    tfm = QTransform().translate(col_x_offset, y_offset)
                    painter.setTransform(tfm)
                    painter.drawLine(
                            QPoint(border_width, 0),
                            QPoint(self._col_width - border_width - 1, 0))

        # Set up paint device for the actual cursor
        tfm = QTransform().translate(x_offset + border_width, y_offset)
        painter.setTransform(tfm)

        # Draw the horizontal line
        line_colour = self._config['edit_cursor']['view_line_colour']
        if self._sheet_mgr.is_editing_enabled():
            line_colour = self._config['edit_cursor']['edit_line_colour']
        painter.setPen(line_colour)
        painter.drawLine(
                QPoint(0, 0),
                QPoint(self._col_width - border_width * 2 - 1, 0))

        # Get trigger row at cursor
        column = self._sheet_mgr.get_column_at_location(location)

        try:
            # Draw the trigger row
            trigger_count = column.get_trigger_count_at_row(row_ts)
            triggers = [column.get_trigger(row_ts, i)
                    for i in range(trigger_count)]
            self._draw_trigger_row_with_edit_cursor(
                    painter, triggers, trigger_index)

        except KeyError:
            # No triggers, just draw a cursor
            if self._sheet_mgr.get_replace_mode():
                self._draw_hollow_replace_cursor(
                        painter, self._config['trigger']['padding_x'], 0)
            else:
                self._draw_hollow_insert_cursor(
                        painter, self._config['trigger']['padding_x'], 0)

    def _get_hollow_replace_cursor_rect(self):
        style_mgr = self._ui_model.get_style_manager()
        metrics = self._config['font_metrics']
        rect = metrics.tightBoundingRect('a') # Seems to produce an OK width
        rect.setTop(0)
        rect.setBottom(self._config['tr_height'] - style_mgr.get_scaled_size(0.2))
        return rect

    def _draw_hollow_replace_cursor(self, painter, x_offset, y_offset):
        rect = self._get_hollow_replace_cursor_rect()
        rect.translate(x_offset, y_offset)
        painter.setPen(self._config['trigger']['default_colour'])
        painter.drawRect(rect)

    def _draw_insert_cursor(self, painter, x_offset, y_offset):
        style_mgr = self._ui_model.get_style_manager()
        width = style_mgr.get_scaled_size(0.2)
        height = self._config['tr_height'] - style_mgr.get_scaled_size(0.15)
        rect = QRect(QPoint(0, 0), QPoint(width, height))
        rect.translate(x_offset, y_offset)
        painter.fillRect(rect, self._config['trigger']['default_colour'])

    def _draw_hollow_insert_cursor(self, painter, x_offset, y_offset):
        style_mgr = self._ui_model.get_style_manager()
        width = style_mgr.get_scaled_size(0.2)
        height = self._config['tr_height'] - style_mgr.get_scaled_size(0.2)
        rect = QRect(QPoint(0, 0), QPoint(width, height))
        rect.translate(x_offset, y_offset)
        painter.setPen(self._config['trigger']['default_colour'])
        painter.drawRect(rect)

    def _draw_trigger_row_with_edit_cursor(self, painter, triggers, trigger_index):
        painter.save()

        border_width = self._config['border_width']
        vis_width = self._col_width - border_width * 2

        painter.setClipRect(
                QRect(QPoint(0, 0), QPoint(vis_width - 1, self._config['tr_height'])))

        # Hide underlying column contents
        painter.fillRect(
                QRect(QPoint(0, 1), QPoint(vis_width, self._config['tr_height'] - 1)),
                self._config['bg_colour'])

        notation = self._notation_mgr.get_selected_notation()
        force_shift = self._ui_model.get_module().get_force_shift()
        rends = [TriggerRenderer(
            self._trigger_cache, self._config, trigger, notation, force_shift)
            for trigger in triggers]
        widths = [r.get_total_width() for r in rends]
        total_width = sum(widths)

        trigger_tfm = painter.transform().translate(-self._trow_px_offset, 0)
        painter.setTransform(trigger_tfm)

        orig_trow_tfm = QTransform(trigger_tfm)

        for i, trigger, renderer in zip(range(len(triggers)), triggers, rends):
            # Identify selected field
            if self._sheet_mgr.get_replace_mode():
                select_replace = (i == trigger_index)
            else:
                if i == trigger_index:
                    self._draw_insert_cursor(painter, 0, 0)
                select_replace = False

            # Render
            renderer.draw_trigger(painter, False, select_replace)

            # Update transform
            trigger_tfm = trigger_tfm.translate(renderer.get_total_width(), 0)
            painter.setTransform(trigger_tfm)

        if trigger_index >= len(triggers):
            # Draw cursor at the end of the row
            if self._sheet_mgr.get_replace_mode():
                self._draw_hollow_replace_cursor(painter, 0, 0)
            else:
                self._draw_hollow_insert_cursor(painter, 0, 0)

        # Draw selected trigger row slice
        selection = self._ui_model.get_selection()
        if selection.has_trigger_row_slice():
            start = selection.get_area_top_left().get_trigger_index()
            stop = selection.get_area_bottom_right().get_trigger_index()
            start_x = sum(r.get_total_width() for r in rends[:start])
            stop_x = start_x + sum(r.get_total_width() for r in rends[start:stop])
            rect = QRect(
                    QPoint(start_x, 0), QPoint(stop_x, self._config['tr_height'] - 1))

            painter.setTransform(orig_trow_tfm)
            painter.setPen(self._config['area_selection']['border_colour'])
            painter.setBrush(self._config['area_selection']['fill_colour'])
            painter.drawRect(rect)

        painter.restore()

    def _draw_selected_area_rect(
            self, painter, selection, rel_draw_col_start, rel_draw_col_stop):
        painter.save()

        draw_col_start = rel_draw_col_start + self._first_col
        draw_col_stop = rel_draw_col_stop + self._first_col

        top_left = selection.get_area_top_left()
        bottom_right = selection.get_area_bottom_right()

        first_area_col = top_left.get_col_num()
        last_area_col = bottom_right.get_col_num()

        start_y = self._get_row_offset(top_left)
        stop_y = self._get_row_offset(bottom_right)
        assert start_y != None
        assert stop_y != None

        border_width = self._config['border_width']

        area_col_start = max(first_area_col, draw_col_start)
        area_col_stop = min(last_area_col + 1, draw_col_stop)
        x_offset = self._get_col_offset(area_col_start)
        painter.setTransform(QTransform().translate(x_offset, 0))
        rect = QRect(
                QPoint(border_width, start_y),
                QPoint(self._col_width - border_width - 1, stop_y))

        painter.setPen(self._config['area_selection']['border_colour'])
        top_left = rect.topLeft()
        top_right = rect.topRight()
        bottom_left = rect.bottomLeft()
        bottom_right = rect.bottomRight()

        for col_index in range(area_col_start, area_col_stop):
            painter.fillRect(rect, self._config['area_selection']['fill_colour'])
            painter.drawLine(top_left, top_right)
            if col_index == first_area_col:
                painter.drawLine(top_left, bottom_left)
            if col_index == last_area_col:
                painter.drawLine(top_right, bottom_right)
            painter.translate(QPoint(self._col_width, 0))

        painter.restore()

    def _move_edit_cursor_trow(self):
        delta = self._horizontal_move_state.get_delta()
        assert delta != 0

        module = self._ui_model.get_module()
        album = module.get_album()
        if not album or album.get_track_count() == 0:
            return

        # Get location info
        selection = self._ui_model.get_selection()
        location = selection.get_location()
        track = location.get_track()
        system = location.get_system()
        col_num = location.get_col_num()
        row_ts = location.get_row_ts()
        trigger_index = location.get_trigger_index()

        cur_column = self._sheet_mgr.get_column_at_location(location)

        if row_ts not in cur_column.get_trigger_row_positions():
            # No triggers
            return

        notation = self._notation_mgr.get_selected_notation()

        if delta < 0:
            if trigger_index == 0:
                # Already at the start of the row
                return

            # Previous trigger
            prev_trigger_index = trigger_index - 1
            new_location = TriggerPosition(
                    track, system, col_num, row_ts, prev_trigger_index)
            selection.set_location(new_location)
            return

        elif delta > 0:
            if trigger_index >= cur_column.get_trigger_count_at_row(row_ts):
                # Already at the end of the row
                return

            # Next trigger
            next_trigger_index = trigger_index + 1
            new_location = TriggerPosition(
                    track, system, col_num, row_ts, next_trigger_index)
            selection.set_location(new_location)
            return

    def _move_edit_cursor_trigger_index(self, index):
        module = self._ui_model.get_module()
        album = module.get_album()
        if not album or album.get_track_count() == 0:
            return

        selection = self._ui_model.get_selection()
        location = selection.get_location()

        test_location = TriggerPosition(
                location.get_track(),
                location.get_system(),
                location.get_col_num(),
                location.get_row_ts(),
                index)
        new_location = self._sheet_mgr.get_clamped_location(test_location)

        selection.set_location(new_location)

    def _move_edit_cursor_column(self, delta):
        assert delta != 0

        module = self._ui_model.get_module()
        album = module.get_album()
        if not album or album.get_track_count() == 0:
            return

        # Get location info
        selection = self._ui_model.get_selection()
        location = selection.get_location()
        track = location.get_track()
        system = location.get_system()
        col_num = location.get_col_num()
        row_ts = location.get_row_ts()
        trigger_index = location.get_trigger_index()

        new_col_num = min(max(0, col_num + delta), COLUMNS_MAX - 1)

        test_location = TriggerPosition(track, system, new_col_num, row_ts, 0)
        new_location = self._sheet_mgr.get_clamped_location(test_location)
        selection.set_location(new_location)

    def _move_edit_cursor_tstamp(self):
        px_delta = self._vertical_move_state.get_delta()
        if px_delta == 0:
            return

        module = self._ui_model.get_module()
        album = module.get_album()
        if not album or album.get_track_count() == 0:
            return

        # Get location info
        selection = self._ui_model.get_selection()
        location = selection.get_location()
        track = location.get_track()
        system = location.get_system()
        col_num = location.get_col_num()
        row_ts = location.get_row_ts()
        trigger_index = location.get_trigger_index()

        stay_within_pattern = selection.has_area_start()

        # Check moving to the previous system
        move_to_previous_system = False
        if px_delta < 0 and row_ts == 0:
            if (track == 0 and system == 0) or stay_within_pattern:
                return
            else:
                move_to_previous_system = True

                new_track = track
                new_system = system - 1
                new_song = album.get_song_by_track(new_track)
                if new_system < 0:
                    new_track -= 1
                    new_song = album.get_song_by_track(new_track)
                    new_system = new_song.get_system_count() - 1
                new_pattern = new_song.get_pattern_instance(new_system).get_pattern()
                pat_height = utils.get_pat_height(
                        new_pattern.get_length(), self._px_per_beat)
                new_ts = new_pattern.get_length()

                track = new_track
                system = new_system
                row_ts = new_ts

                location = TriggerPosition(
                        track, system, col_num, row_ts, trigger_index)

        cur_column = self._sheet_mgr.get_column_at_location(location)
        is_grid_enabled = self._sheet_mgr.is_grid_enabled()

        if is_grid_enabled:
            grid = self._sheet_mgr.get_grid()
            song = album.get_song_by_track(track)
            pinst = song.get_pattern_instance(system)
            tr_height_ts = utils.get_tstamp_from_px(
                    self._config['tr_height'], self._px_per_beat)

            # Get closest grid line in our target direction
            if px_delta < 0:
                line_info = grid.get_prev_line(pinst, col_num, row_ts, tr_height_ts)
                if line_info:
                    new_ts, _ = line_info
                else:
                    new_ts = tstamp.Tstamp(0)
            else:
                line_info = grid.get_next_line(pinst, col_num, row_ts, tr_height_ts)
                if line_info:
                    new_ts, _ = line_info
                else:
                    pattern = pinst.get_pattern()
                    new_ts = pattern.get_length()

            if line_info:
                new_ts, _ = line_info
        else:
            # Get default trigger tstamp on the current pixel position
            cur_px_offset = utils.get_px_from_tstamp(row_ts, self._px_per_beat)
            def_ts = utils.get_tstamp_from_px(cur_px_offset, self._px_per_beat)
            assert utils.get_px_from_tstamp(def_ts, self._px_per_beat) == cur_px_offset

            # Get target tstamp
            new_px_offset = cur_px_offset + px_delta
            new_ts = utils.get_tstamp_from_px(new_px_offset, self._px_per_beat)
            assert utils.get_px_from_tstamp(new_ts, self._px_per_beat) != cur_px_offset

        # Get shortest movement between target tstamp and closest trigger row
        move_range_start = min(new_ts, row_ts)
        move_range_stop = max(new_ts, row_ts) + tstamp.Tstamp(0, 1)
        if px_delta < 0:
            if not move_to_previous_system:
                move_range_stop -= tstamp.Tstamp(0, 1)
        else:
            move_range_start += tstamp.Tstamp(0, 1)

        trow_tstamps = cur_column.get_trigger_row_positions_in_range(
                move_range_start, move_range_stop)
        if trow_tstamps:
            if not is_grid_enabled:
                self._vertical_move_state.try_snap_delay()

            if px_delta < 0:
                new_ts = max(trow_tstamps)
            else:
                new_ts = min(trow_tstamps)

        # Check moving outside pattern boundaries
        cur_song = album.get_song_by_track(track)
        cur_pattern = cur_song.get_pattern_instance(system).get_pattern()

        if new_ts <= 0:
            if not is_grid_enabled:
                self._vertical_move_state.try_snap_delay()
            new_ts = tstamp.Tstamp(0)
        elif new_ts > cur_pattern.get_length():
            if stay_within_pattern:
                new_ts = cur_pattern.get_length()
                new_location = TriggerPosition(
                        track, system, col_num, new_ts, 0)
                selection.set_location(new_location)
                return

            new_track = track
            new_system = system + 1
            if new_system >= cur_song.get_system_count():
                new_track += 1
                new_system = 0
                if new_track >= album.get_track_count():
                    # End of sheet
                    new_ts = cur_pattern.get_length()
                    new_location = TriggerPosition(
                            track, system, col_num, new_ts, 0)
                    selection.set_location(new_location)
                    return

            # Next pattern
            if not is_grid_enabled:
                self._vertical_move_state.try_snap_delay()
            new_ts = tstamp.Tstamp(0)
            new_location = TriggerPosition(
                    new_track, new_system, col_num, new_ts, 0)
            selection.set_location(new_location)
            return

        # Move inside pattern
        new_location = TriggerPosition(track, system, col_num, new_ts, 0)
        selection.set_location(new_location)

    def _move_edit_cursor_bar(self, delta):
        assert delta in (-1, 1)

        module = self._ui_model.get_module()
        album = module.get_album()
        if not album or album.get_track_count() == 0:
            return

        # Get location info
        selection = self._ui_model.get_selection()
        location = selection.get_location()
        track = location.get_track()
        system = location.get_system()
        col_num = location.get_col_num()
        row_ts = location.get_row_ts()
        trigger_index = location.get_trigger_index()

        cur_song = album.get_song_by_track(track)
        cur_pattern = cur_song.get_pattern_instance(system).get_pattern()
        cur_pat_length = cur_pattern.get_length()

        # Use grid pattern length as our step
        gp_id = cur_pattern.get_base_grid_pattern_id()
        grid_mgr = self._ui_model.get_grid_manager()
        gp = grid_mgr.get_grid_pattern(gp_id)
        page_step = gp.get_length()

        new_ts = row_ts + page_step * delta

        stay_within_pattern = selection.has_area_start()
        if stay_within_pattern:
            new_ts = min(max(tstamp.Tstamp(0), new_ts), cur_pat_length)

        if new_ts < 0:
            new_track = track
            new_system = system - 1
            if new_system < 0:
                new_track -= 1
                if new_track < 0:
                    # Start of sheet
                    new_track = 0
                    new_system = 0
                    new_ts = tstamp.Tstamp(0)
                    new_location = TriggerPosition(
                            new_track, new_system, col_num, new_ts, 0)
                    selection.set_location(new_location)
                    return
                else:
                    new_song = album.get_song_by_track(new_track)
                    new_system = new_song.get_system_count() - 1

            # Previous pattern
            new_song = album.get_song_by_track(new_track)
            new_pattern = new_song.get_pattern_instance(new_system).get_pattern()

            new_pat_length = new_pattern.get_length()
            pat_length_rems = new_pat_length.beats * tstamp.BEAT + new_pat_length.rem

            new_gp_id = new_pattern.get_base_grid_pattern_id()
            new_gp = grid_mgr.get_grid_pattern(new_gp_id)
            new_page_step = new_gp.get_length()
            new_page_step_rems = new_page_step.beats * tstamp.BEAT + new_page_step.rem

            full_bar_count = pat_length_rems // new_page_step_rems
            last_bar_start_rems = full_bar_count * new_page_step_rems
            if last_bar_start_rems == pat_length_rems:
                last_bar_start_rems = max(0, last_bar_start_rems - new_page_step_rems)
            last_bar_start = tstamp.Tstamp(0, last_bar_start_rems)

            cur_page_step_rems = page_step.beats * tstamp.BEAT + page_step.rem
            row_ts_rems = row_ts.beats * tstamp.BEAT + row_ts.rem
            bar_offset_rems = row_ts_rems % cur_page_step_rems

            new_ts = last_bar_start + tstamp.Tstamp(0, bar_offset_rems)
            new_ts = min(new_ts, new_pat_length)

            new_location = TriggerPosition(new_track, new_system, col_num, new_ts, 0)
            selection.set_location(new_location)
            return

        elif ((new_ts > cur_pat_length) or
                (cur_pat_length.rem == 0 and
                    new_ts == cur_pat_length and
                    not stay_within_pattern)):
            new_track = track
            new_system = system + 1
            if new_system >= cur_song.get_system_count():
                new_track += 1
                new_system = 0
                if new_track >= album.get_track_count():
                    # End of sheet
                    new_ts = cur_pattern.get_length()
                    new_location = TriggerPosition(
                            track, system, col_num, new_ts, 0)
                    selection.set_location(new_location)
                    return

            # Next pattern
            new_song = album.get_song_by_track(new_track)
            new_pattern = new_song.get_pattern_instance(new_system).get_pattern()
            new_ts_rems = new_ts.beats * tstamp.BEAT + new_ts.rem
            pat_length_rems = cur_pat_length.beats * tstamp.BEAT + cur_pat_length.rem
            excess_rems = new_ts_rems - pat_length_rems
            new_ts = tstamp.Tstamp(0, excess_rems)
            new_ts = min(new_ts, new_pattern.get_length())

            new_location = TriggerPosition(new_track, new_system, col_num, new_ts, 0)
            selection.set_location(new_location)
            return

        new_location = TriggerPosition(track, system, col_num, new_ts, 0)
        selection.set_location(new_location)

    def _get_trigger_index(self, column, row_ts, x_offset):
        if not column.has_trigger(row_ts, 0):
            return -1

        vis_x_offset = x_offset - self._config['border_width']

        trigger_count = column.get_trigger_count_at_row(row_ts)
        triggers = (column.get_trigger(row_ts, i)
                for i in range(trigger_count))
        notation = self._notation_mgr.get_selected_notation()
        force_shift = self._ui_model.get_module().get_force_shift()
        rends = (TriggerRenderer(
            self._trigger_cache, self._config, trigger, notation, force_shift)
            for trigger in triggers)
        widths = [r.get_total_width() for r in rends]
        init_width = 0
        trigger_index = 0
        for width in widths:
            prev_init_width = init_width
            init_width += width
            if init_width >= vis_x_offset:
                if not self._sheet_mgr.get_replace_mode():
                    if (init_width - vis_x_offset) < (vis_x_offset - prev_init_width):
                        trigger_index += 1
                break
            trigger_index += 1
        else:
            hollow_rect = self._get_hollow_replace_cursor_rect()
            dist_from_last = vis_x_offset - init_width
            trigger_padding = self._config['trigger']['padding_x']
            if dist_from_last > hollow_rect.width() + trigger_padding:
                return -1

        assert trigger_index <= trigger_count
        return trigger_index

    def _get_selected_location(self, view_x_offset, view_y_offset):
        module = self._ui_model.get_module()
        album = module.get_album()
        if not album:
            return None
        track_count = album.get_track_count()
        songs = (album.get_song_by_track(i) for i in range(track_count))
        if not songs:
            return None

        view_y_offset = max(0, view_y_offset)

        # Get column number
        col_num = max(0, self._first_col + (view_x_offset // self._col_width))
        if col_num >= COLUMNS_MAX:
            return None
        rel_x_offset = view_x_offset % self._col_width

        # Get pattern index
        y_offset = self._px_offset + view_y_offset
        pat_index = utils.get_first_visible_pat_index(y_offset, self._start_heights)
        pat_index = min(pat_index, len(self._pinsts) - 1)

        # Get track and system
        track = -1
        system = pat_index
        for song in songs:
            track += 1
            system_count = song.get_system_count()
            if system >= system_count:
                system -= system_count
            else:
                break
        if track < 0:
            return None

        # Get row timestamp
        rel_y_offset = y_offset - self._start_heights[pat_index]
        assert rel_y_offset >= 0
        row_ts = utils.get_tstamp_from_px(rel_y_offset, self._px_per_beat)
        row_ts = min(row_ts, self._pinsts[pat_index].get_pattern().get_length())

        # Get current selection info
        selection = self._ui_model.get_selection()
        cur_location = selection.get_location()
        cur_column = self._sheet_mgr.get_column_at_location(cur_location)

        # Select a trigger if its description overlaps with the mouse cursor
        trigger_selected = False
        trigger_index = 0
        tr_track, tr_system = track, system
        tr_pat_index = pat_index
        while tr_pat_index >= 0:
            tr_location = TriggerPosition(
                    tr_track, tr_system, col_num, tstamp.Tstamp(0), 0)
            column = self._sheet_mgr.get_column_at_location(tr_location)
            if not column:
                break

            # Get range for checking
            start_ts = tstamp.Tstamp(0)
            if tr_pat_index == pat_index:
                stop_ts = row_ts
                tr_rel_y_offset = rel_y_offset
            else:
                stop_ts = self._pinsts[tr_pat_index].get_pattern().get_length()
                tr_rel_y_offset = self._heights[tr_pat_index] + rel_y_offset - 1
            stop_ts += tstamp.Tstamp(0, 1)

            # Get check location
            trow_tstamps = column.get_trigger_row_positions_in_range(
                    start_ts, stop_ts)
            if trow_tstamps:
                check_ts = max(trow_tstamps)
            else:
                check_ts = tstamp.Tstamp(0)

            # Get pixel distance to the click position
            check_y_offset = utils.get_px_from_tstamp(check_ts, self._px_per_beat)
            y_dist = tr_rel_y_offset - check_y_offset
            assert y_dist >= 0
            is_close_enough = (y_dist < self._config['tr_height'] - 1)
            if not is_close_enough:
                break

            # Override check location if we clicked on a currently overlaid trigger
            if (cur_column and
                    cur_location.get_track() <= tr_track and
                    cur_location.get_col_num() == col_num):
                # Get current location offset
                cur_track = cur_location.get_track()
                cur_system = cur_location.get_system()
                cur_pat_index = utils.get_pattern_index_at_location(
                        self._ui_model, cur_track, cur_system)

                cur_ts = cur_location.get_row_ts()
                cur_pat_y_offset = utils.get_px_from_tstamp(cur_ts, self._px_per_beat)
                cur_y_offset = self._start_heights[cur_pat_index] + cur_pat_y_offset

                tr_y_offset = self._start_heights[pat_index] + tr_rel_y_offset
                cur_y_dist = tr_y_offset - cur_y_offset

                if cur_y_dist >= 0 and cur_y_dist < self._config['tr_height'] - 1:
                    tr_rel_x_offset = rel_x_offset + self._trow_px_offset
                    new_trigger_index = self._get_trigger_index(
                            cur_column, cur_ts, tr_rel_x_offset)
                    if new_trigger_index >= 0:
                        trigger_index = new_trigger_index
                        track, system, row_ts = cur_track, cur_system, cur_ts
                        trigger_selected = True
                        break

            if trow_tstamps:
                # If this trow is already selected, consider additional row offset
                if (cur_location.get_track() == tr_track and
                        cur_location.get_system() == tr_system and
                        cur_location.get_col_num() == col_num and
                        cur_location.get_row_ts() == check_ts):
                    tr_rel_x_offset = rel_x_offset + self._trow_px_offset
                else:
                    tr_rel_x_offset = rel_x_offset

                # Get trigger index
                new_trigger_index = self._get_trigger_index(
                        column, check_ts, tr_rel_x_offset)
                if new_trigger_index >= 0:
                    trigger_index = new_trigger_index
                    track, system, row_ts = tr_track, tr_system, check_ts
                    trigger_selected = True
                    break

            # Check previous system
            tr_pat_index -= 1
            tr_system -= 1
            while tr_system < 0:
                tr_track -= 1
                if tr_track < 0:
                    assert tr_pat_index < 0
                    break
                song = album.get_song_by_track(tr_track)
                tr_system = song.get_system_count() - 1

        # Make sure we snap to something if grid is enabled
        if (not trigger_selected) and self._sheet_mgr.is_grid_enabled():
            grid = self._sheet_mgr.get_grid()
            tr_height_ts = utils.get_tstamp_from_px(
                    self._config['tr_height'], self._px_per_beat)

            cur_song = album.get_song_by_track(track)
            cur_pinst = cur_song.get_pattern_instance(system)
            cur_pattern = cur_pinst.get_pattern()

            # Select grid line above if an infinite trigger row would
            # overlap with the click position
            prev_line_selected = False
            prev_ts = tstamp.Tstamp(0)
            prev_line_info = grid.get_prev_or_current_line(
                    cur_pinst, col_num, row_ts, tr_height_ts)
            if prev_line_info:
                prev_ts, _ = prev_line_info
                prev_y_offset = utils.get_px_from_tstamp(prev_ts, self._px_per_beat)
                cur_y_offset = utils.get_px_from_tstamp(row_ts, self._px_per_beat)
                y_dist = cur_y_offset - prev_y_offset
                assert y_dist >= 0
                is_close_enough = (y_dist < self._config['tr_height'] - 1)
                if is_close_enough:
                    row_ts = prev_ts
                    prev_line_selected = True

            if not prev_line_selected:
                # Get whatever trigger row or grid line is nearest
                next_ts = cur_pattern.get_length()
                next_line_info = grid.get_next_or_current_line(
                        cur_pinst, col_num, row_ts, tr_height_ts)
                if next_line_info:
                    next_ts, _ = next_line_info

                cur_column = cur_pinst.get_column(col_num)

                # Get nearest previous target timestamp
                prev_tstamps = cur_column.get_trigger_row_positions_in_range(
                        prev_ts, row_ts)
                if prev_tstamps:
                    prev_ts = max(prev_tstamps)

                # Get nearest next target timestamp
                next_tstamps = cur_column.get_trigger_row_positions_in_range(
                        row_ts, next_ts)
                if next_tstamps:
                    next_ts = min(next_tstamps)

                # Get nearest of the two timestamps
                if (row_ts - prev_ts) < (next_ts - row_ts):
                    row_ts = prev_ts
                else:
                    row_ts = next_ts

        location = TriggerPosition(track, system, col_num, row_ts, trigger_index)
        return location

    def _handle_cursor_down_with_grid(self):
        # TODO: fix this mess
        is_editing_grid_enabled = (self._sheet_mgr.is_editing_enabled() and
                self._sheet_mgr.is_grid_enabled())
        if is_editing_grid_enabled:
            self._vertical_move_state.press_down()
            self._move_edit_cursor_tstamp()
            self._vertical_move_state.release_down()

    def _add_rest(self):
        trigger = Trigger('n-', None)
        self._sheet_mgr.add_trigger(trigger)
        self._handle_cursor_down_with_grid()

    def _try_delete_selection(self):
        self._sheet_mgr.try_remove_trigger()

    def _perform_delete(self):
        if not self._sheet_mgr.is_editing_enabled():
            return

        selection = self._ui_model.get_selection()
        if selection.has_area():
            self._sheet_mgr.try_remove_area()
        else:
            self._try_delete_selection()
            self._handle_cursor_down_with_grid()

    def _perform_backspace(self):
        if not self._sheet_mgr.is_editing_enabled():
            return

        module = self._ui_model.get_module()
        album = module.get_album()
        if not album or album.get_track_count() == 0:
            return

        selection = self._ui_model.get_selection()
        location = selection.get_location()
        trigger_index = location.get_trigger_index()
        if trigger_index > 0:
            self._move_edit_cursor_trigger_index(trigger_index - 1)
            self._try_delete_selection()

    def _get_selected_coordinates(self):
        selection = self._ui_model.get_selection()
        location = selection.get_location()

        x_offset = self._get_col_offset(location.get_col_num())
        y_offset = self._get_row_offset(location)

        column = self._sheet_mgr.get_column_at_location(location)
        if not column:
            return None
        row_ts = location.get_row_ts()
        notation = self._notation_mgr.get_selected_notation()
        force_shift = self._ui_model.get_module().get_force_shift()

        try:
            trigger_count = column.get_trigger_count_at_row(row_ts)
            triggers = [column.get_trigger(row_ts, i)
                    for i in range(min(trigger_count, location.get_trigger_index()))]
            rends = [TriggerRenderer(
                self._trigger_cache, self._config, trigger, notation, force_shift)
                for trigger in triggers]
            widths = [r.get_total_width() for r in rends]
            init_width = sum(widths)
            x_offset += init_width
        except KeyError:
            pass

        x_offset -= self._trow_px_offset

        return (x_offset, y_offset)

    def _start_trigger_type_entry(self):
        if not self._sheet_mgr.is_editing_enabled() or self._playback_mgr.is_recording():
            return

        self._follow_edit_cursor()

        coords = self._get_selected_coordinates()
        if not coords:
            return

        x_offset, y_offset = coords

        validator = TriggerTypeValidator()
        self._field_edit.start_editing(
                x_offset, y_offset, validator, self._finish_trigger_type_entry)
        self._field_edit.show()
        self._field_edit.setFocus()

    def _get_example_trigger_argument(self, trigger_type):
        event_name = trigger_type.split(':')[0]
        info = events.trigger_events_by_name[event_name]

        # Special case: tempo values
        if info['name'] in ('m.t', 'm/t'):
            return '120'

        ex = {
            None                            : None,
            events.EVENT_ARG_BOOL           : 'false',
            events.EVENT_ARG_INT            : '0',
            events.EVENT_ARG_FLOAT          : '0',
            events.EVENT_ARG_TSTAMP         : '0',
            events.EVENT_ARG_STRING         : "''",
            events.EVENT_ARG_PAT            : 'pat(0, 0)',
            events.EVENT_ARG_PITCH          : '0',
            events.EVENT_ARG_REALTIME       : '0',
            events.EVENT_ARG_MAYBE_STRING   : None,
            events.EVENT_ARG_MAYBE_REALTIME : None,
        }

        return ex[info['arg_type']]

    def _finish_trigger_type_entry(self, text):
        selection = self._ui_model.get_selection()
        orig_location = selection.get_location()

        self.setFocus()
        arg = self._get_example_trigger_argument(text)
        event_name = text.split(':')[0]
        arg_type = events.trigger_events_by_name[event_name]['arg_type']
        is_immediate_arg_type = arg_type in (None, events.EVENT_ARG_PITCH)

        trigger = Trigger(text, arg)
        self._sheet_mgr.add_trigger(trigger, commit=is_immediate_arg_type)

        if not is_immediate_arg_type:
            selection.set_location(orig_location)
            self._start_trigger_argument_entry(new=True)

    def _start_trigger_argument_entry(self, new=False):
        if not self._sheet_mgr.is_editing_enabled() or self._playback_mgr.is_recording():
            return

        self._follow_edit_cursor()

        coords = self._get_selected_coordinates()
        if not coords:
            return

        x_offset, y_offset = coords

        trigger = self._sheet_mgr.get_selected_trigger()
        if trigger.get_argument_type() == None:
            return

        # Offset field edit so that trigger type remains visible
        if trigger.get_type() not in ('n+', 'h'):
            notation = self._notation_mgr.get_selected_notation()
            force_shift = self._ui_model.get_module().get_force_shift()
            renderer = TriggerRenderer(
                    self._trigger_cache, self._config, trigger, notation, force_shift)
            _, type_width = renderer.get_field_bounds(0)
            x_offset += type_width

        validator = TriggerArgumentValidator()
        if new:
            start_text = self._get_example_trigger_argument(trigger.get_type())
        else:
            start_text = trigger.get_argument()

        if start_text == None:
            start_text = ''

        self._field_edit.start_editing(
                x_offset,
                y_offset,
                validator,
                self._finish_trigger_argument_entry,
                start_text)

    def _finish_trigger_argument_entry(self, text):
        self.setFocus()

        trigger = self._sheet_mgr.get_selected_trigger()
        maybe_types = (events.EVENT_ARG_MAYBE_STRING, events.EVENT_ARG_MAYBE_REALTIME)
        if trigger.get_argument_type() in maybe_types and not text:
            text = None
        new_trigger = Trigger(trigger.get_type(), text)

        if not self._sheet_mgr.get_replace_mode():
            self._sheet_mgr.try_remove_trigger()
        self._sheet_mgr.add_trigger(new_trigger)

    def event(self, event):
        if (event.type() == QEvent.KeyPress and
                event.key() in (Qt.Key_Tab, Qt.Key_Backtab)):
            return self.keyPressEvent(event) or False
        return QWidget.event(self, event)

    def keyPressEvent(self, event):
        selection = self._ui_model.get_selection()
        orig_location = selection.get_location()

        mapper_handled = self._keyboard_mapper.process_typewriter_button_event(event)
        key_action = self._keyboard_mapper.get_key_action(event)
        if mapper_handled:
            if ((key_action.action_type == KeyboardAction.NOTE) and
                    self._sheet_mgr.allow_note_autorepeat()):
                self._handle_cursor_down_with_grid()
            return
        else:
            if key_action and (key_action.action_type == KeyboardAction.REST):
                if (not event.isAutoRepeat()) or self._sheet_mgr.allow_note_autorepeat():
                    self._add_rest()

        allow_editing_operations = (
                not self._playback_mgr.follow_playback_cursor() or
                self._playback_mgr.is_recording())

        if allow_editing_operations:
            if event.key() == Qt.Key_Tab:
                event.accept()
                selection.clear_area()
                self._move_edit_cursor_column(1)
                return True
            elif event.key() == Qt.Key_Backtab:
                event.accept()
                selection.clear_area()
                self._move_edit_cursor_column(-1)
                return True
        else:
            if event.key() == Qt.Key_Tab:
                event.accept()
                max_visible_cols = utils.get_max_visible_cols(
                        self.width(), self._col_width)
                first_col = utils.clamp_start_col(self._first_col + 1, max_visible_cols)
                self.followPlaybackColumn.emit(first_col)
                return True
            elif event.key() == Qt.Key_Backtab:
                event.accept()
                first_col = max(0, self._first_col - 1)
                self.followPlaybackColumn.emit(first_col)
                return True

        def handle_move_up():
            selection.clear_area()
            self._vertical_move_state.press_up()
            self._move_edit_cursor_tstamp()

        def handle_move_down():
            selection.clear_area()
            self._vertical_move_state.press_down()
            self._move_edit_cursor_tstamp()

        def handle_move_left():
            if selection.has_area_start():
                selection.clear_area()
                self.update()
            self._horizontal_move_state.press_left()
            self._move_edit_cursor_trow()

        def handle_move_right():
            if selection.has_area_start():
                selection.clear_area()
                self.update()
            self._horizontal_move_state.press_right()
            self._move_edit_cursor_trow()

        def handle_move_prev_bar():
            if selection.has_area_start():
                selection.clear_area()
                self.update()
            self._move_edit_cursor_bar(-1)

        def handle_move_next_bar():
            if selection.has_area_start():
                selection.clear_area()
                self.update()
            self._move_edit_cursor_bar(1)

        def handle_move_trow_start():
            if selection.has_area_start():
                selection.clear_area()
                self.update()
            self._move_edit_cursor_trigger_index(0)

        def handle_move_trow_end():
            if selection.has_area_start():
                selection.clear_area()
                self.update()
            self._move_edit_cursor_trigger_index(2**24) # :-P

        def area_bounds_move_up():
            selection.try_set_area_start(orig_location)
            self._vertical_move_state.press_up()
            self._move_edit_cursor_tstamp()
            selection.set_area_stop(selection.get_location())

        def area_bounds_move_down():
            selection.try_set_area_start(orig_location)
            self._vertical_move_state.press_down()
            self._move_edit_cursor_tstamp()
            selection.set_area_stop(selection.get_location())

        def area_bounds_move_left():
            selection.try_set_area_start(orig_location)
            if selection.has_rect_area() or not self._sheet_mgr.is_at_trigger_row():
                self._move_edit_cursor_column(-1)
            else:
                self._horizontal_move_state.press_left()
                self._move_edit_cursor_trow()
            selection.set_area_stop(selection.get_location())

        def area_bounds_move_right():
            selection.try_set_area_start(orig_location)
            if selection.has_rect_area() or not self._sheet_mgr.is_at_trigger_row():
                self._move_edit_cursor_column(1)
            else:
                self._horizontal_move_state.press_right()
                self._move_edit_cursor_trow()
            selection.set_area_stop(selection.get_location())

        def area_bounds_move_prev_bar():
            selection.try_set_area_start(orig_location)
            self._move_edit_cursor_bar(-1)
            selection.set_area_stop(selection.get_location())

        def area_bounds_move_next_bar():
            selection.try_set_area_start(orig_location)
            self._move_edit_cursor_bar(1)
            selection.set_area_stop(selection.get_location())

        def area_bounds_move_trow_start():
            if not selection.has_rect_area():
                selection.try_set_area_start(orig_location)
                self._move_edit_cursor_trigger_index(0)
                selection.set_area_stop(selection.get_location())

        def area_bounds_move_trow_end():
            if not selection.has_rect_area():
                selection.try_set_area_start(orig_location)
                self._move_edit_cursor_trigger_index(2**24) # :-P
                selection.set_area_stop(selection.get_location())

        def area_select_all():
            location = selection.get_location()
            module = self._ui_model.get_module()
            album = module.get_album()
            song = album.get_song_by_track(location.get_track())
            pinst = song.get_pattern_instance(location.get_system())
            pattern = pinst.get_pattern()
            selection.clear_area()
            selection.try_set_area_start(TriggerPosition(
                location.get_track(),
                location.get_system(),
                0,
                tstamp.Tstamp(0),
                0))
            selection.set_area_stop(TriggerPosition(
                location.get_track(),
                location.get_system(),
                COLUMNS_MAX - 1,
                pattern.get_length() + tstamp.Tstamp(0, 1),
                0))
            self.update()

        def area_select_columns():
            if selection.has_area():
                top_left = selection.get_area_top_left()
                bottom_right = selection.get_area_bottom_right()
            else:
                top_left = selection.get_location()
                bottom_right = top_left

            module = self._ui_model.get_module()
            album = module.get_album()
            song = album.get_song_by_track(top_left.get_track())
            pinst = song.get_pattern_instance(top_left.get_system())
            pattern = pinst.get_pattern()

            selection.clear_area()
            selection.try_set_area_start(TriggerPosition(
                top_left.get_track(),
                top_left.get_system(),
                top_left.get_col_num(),
                tstamp.Tstamp(0),
                0))
            selection.set_area_stop(TriggerPosition(
                bottom_right.get_track(),
                bottom_right.get_system(),
                bottom_right.get_col_num(),
                pattern.get_length() + tstamp.Tstamp(0, 1),
                0))
            self.update()

        def area_copy():
            if selection.has_area():
                utils.copy_selected_area(self._sheet_mgr)
                selection.clear_area()
                self.update()

        def area_cut():
            if selection.has_area():
                utils.copy_selected_area(self._sheet_mgr)
                self._sheet_mgr.try_remove_area()
                selection.clear_area()

        def area_paste():
            utils.try_paste_area(self._sheet_mgr)
            selection.clear_area()

        def handle_typewriter_connection():
            if not event.isAutoRepeat():
                connected = self._sheet_mgr.get_typewriter_connected()
                self._sheet_mgr.set_typewriter_connected(not connected)

        def handle_replace_mode():
            if not event.isAutoRepeat():
                is_replace = self._sheet_mgr.get_replace_mode()
                self._sheet_mgr.set_replace_mode(not is_replace)

        def handle_field_edit():
            if self._sheet_mgr.get_replace_mode() and self._sheet_mgr.is_at_trigger():
                self._start_trigger_argument_entry()
            else:
                self._start_trigger_type_entry()

        def handle_convert_trigger():
            if (self._sheet_mgr.is_editing_enabled() and
                    self._sheet_mgr.is_at_convertible_set_or_slide_trigger()):
                self._sheet_mgr.convert_set_or_slide_trigger()

        def handle_undo():
            history = self._ui_model.get_sheet_history()
            history.undo()

        def handle_redo():
            history = self._ui_model.get_sheet_history()
            history.redo()

        def handle_zoom(new_zoom):
            if self._sheet_mgr.set_zoom(new_zoom):
                self._updater.signal_update('signal_sheet_zoom')

        def handle_column_width(new_width):
            if self._sheet_mgr.set_column_width(new_width):
                self._updater.signal_update('signal_sheet_column_width')

        def handle_playback_marker():
            cur_location = self._ui_model.get_selection().get_location()
            cur_track = cur_location.get_track()
            cur_system = cur_location.get_system()
            cur_row_ts = cur_location.get_row_ts()

            cur_marker = self._playback_mgr.get_playback_marker()
            if (not cur_marker) or (cur_marker != (cur_track, cur_system, cur_row_ts)):
                self._playback_mgr.set_playback_marker(cur_track, cur_system, cur_row_ts)
            else:
                self._playback_mgr.clear_playback_marker()

            self._updater.signal_update('signal_playback_marker')

        keymap = {
            int(Qt.NoModifier): {
            },

            int(Qt.ControlModifier): {
                Qt.Key_Minus:   lambda: handle_zoom(self._sheet_mgr.get_zoom() - 1),
                Qt.Key_Plus:    lambda: handle_zoom(self._sheet_mgr.get_zoom() + 1),
                Qt.Key_0:       lambda: handle_zoom(0),
            },

            int(Qt.ControlModifier | Qt.ShiftModifier): {
            },

            int(Qt.ControlModifier | Qt.AltModifier): {
                Qt.Key_Minus:   lambda: handle_column_width(
                                    self._sheet_mgr.get_column_width() - 1),
                Qt.Key_Plus:    lambda: handle_column_width(
                                    self._sheet_mgr.get_column_width() + 1),
                Qt.Key_0:       lambda: handle_column_width(0),
                Qt.Key_Comma:   handle_playback_marker,
            },

            int(Qt.ShiftModifier): {
            },
        }

        edit_keymap = {
            int(Qt.NoModifier): {
                Qt.Key_Up:      handle_move_up,
                Qt.Key_Down:    handle_move_down,
                Qt.Key_Left:    handle_move_left,
                Qt.Key_Right:   handle_move_right,

                Qt.Key_PageUp:  handle_move_prev_bar,
                Qt.Key_PageDown: handle_move_next_bar,

                Qt.Key_Home:    handle_move_trow_start,
                Qt.Key_End:     handle_move_trow_end,

                Qt.Key_Delete:  lambda: self._perform_delete(),
                Qt.Key_Backspace: lambda: self._perform_backspace(),

                Qt.Key_Space:   handle_typewriter_connection,
                Qt.Key_Insert:  handle_replace_mode,
                Qt.Key_Escape:  lambda: self._sheet_mgr.set_typewriter_connected(False),

                Qt.Key_Return:  handle_field_edit,
            },

            int(Qt.ControlModifier): {
                Qt.Key_A:       area_select_all,
                Qt.Key_L:       area_select_columns,
                Qt.Key_X:       area_cut,
                Qt.Key_C:       area_copy,
                Qt.Key_V:       area_paste,
                Qt.Key_M:       handle_convert_trigger,
                Qt.Key_Z:       handle_undo,
            },

            int(Qt.ControlModifier | Qt.ShiftModifier): {
                Qt.Key_Z:       handle_redo,
            },

            int(Qt.ControlModifier | Qt.AltModifier): {
            },

            int(Qt.ShiftModifier): {
                Qt.Key_Up:      area_bounds_move_up,
                Qt.Key_Down:    area_bounds_move_down,
                Qt.Key_Left:    area_bounds_move_left,
                Qt.Key_Right:   area_bounds_move_right,
                Qt.Key_PageUp:  area_bounds_move_prev_bar,
                Qt.Key_PageDown: area_bounds_move_next_bar,
                Qt.Key_Home:    area_bounds_move_trow_start,
                Qt.Key_End:     area_bounds_move_trow_end,
            },
        }

        if allow_editing_operations:
            for k in edit_keymap.keys():
                keymap[k].update(edit_keymap[k])

        is_handled = False
        mod_map = keymap.get(int(event.modifiers()))
        if mod_map:
            func = mod_map.get(event.key())
            if func:
                func()
                is_handled = True

        # Prevent propagation of disabled key combinations
        if not is_handled:
            disabled_mod_map = edit_keymap.get(int(event.modifiers()), {})
            if disabled_mod_map.get(event.key()):
                is_handled = True

        if not is_handled:
            QWidget.keyPressEvent(self, event)

    def keyReleaseEvent(self, event):
        was_chord_mode = self._sheet_mgr.get_chord_mode()

        if self._keyboard_mapper.process_typewriter_button_event(event):
            is_chord_mode = self._sheet_mgr.get_chord_mode()
            if was_chord_mode and not is_chord_mode:
                self._handle_cursor_down_with_grid()
            return

        if event.isAutoRepeat():
            return

        if event.key() == Qt.Key_Up:
            self._vertical_move_state.release_up()
        elif event.key() == Qt.Key_Down:
            self._vertical_move_state.release_down()
        elif event.key() == Qt.Key_Left:
            self._horizontal_move_state.release_left()
        elif event.key() == Qt.Key_Right:
            self._horizontal_move_state.release_right()

    def resizeEvent(self, event):
        max_visible_cols = utils.get_max_visible_cols(self.width(), self._col_width)
        first_col = utils.clamp_start_col(self._first_col, max_visible_cols)
        visible_cols = utils.get_visible_cols(first_col, max_visible_cols)

        update_rect = None

        if first_col != self._first_col:
            update_rect = QRect(0, 0, self.width(), self.height())
        elif visible_cols > self._visible_cols:
            x_offset = self._visible_cols * self._col_width
            width = self.width() - x_offset
            update_rect = QRect(x_offset, 0, width, self.height())

        self._first_col = first_col
        self._visible_cols = visible_cols

        if event.size().height() > event.oldSize().height():
            update_rect = QRect(0, 0, self.width(), self.height())

        if update_rect:
            self.update(update_rect)

    def paintEvent(self, event):
        start = time.time()

        painter = QPainter(self)
        painter.setBackground(self._config['canvas_bg_colour'])

        rect = event.rect()

        old_trigger_count = self._trigger_cache.trigger_count()

        # See which columns should be drawn
        draw_col_start = rect.x() // self._col_width
        draw_col_stop = 1 + (rect.x() + rect.width() - 1) // self._col_width
        draw_col_stop = min(draw_col_stop, COLUMNS_MAX - self._first_col)

        # Get grid (moved here from ColumnGroupRenderer for cached data access)
        grid = self._sheet_mgr.get_grid()

        # Draw columns
        pixmaps_created = 0
        for rel_col_index in range(draw_col_start, draw_col_stop):
            x_offset = rel_col_index * self._col_width
            tfm = QTransform().translate(x_offset, 0)
            painter.setTransform(tfm)
            pixmaps_created += self._col_rends[
                    self._first_col + rel_col_index].draw(painter, self.height(), grid)

        # Flush caches of (most) out-of-view columns
        first_kept_col = max(0, self._first_col - 1)
        last_kept_col = min(COLUMNS_MAX - 1, self._first_col + draw_col_stop)
        for col_index in range(self._prev_first_kept_col, self._prev_last_kept_col + 1):
            if not (first_kept_col <= col_index <= last_kept_col):
                self._col_rends[col_index].flush_caches()
        self._prev_first_kept_col = first_kept_col
        self._prev_last_kept_col = last_kept_col

        painter.setTransform(QTransform())

        # Fill horizontal trailing blank
        hor_trail_start = draw_col_stop * self._col_width
        if hor_trail_start < self.width():
            width = self.width() - hor_trail_start
            painter.eraseRect(QRect(hor_trail_start, 0, width, self.height()))

        # Draw playback cursor
        if self._is_playback_cursor_visible:
            painter.setPen(self._config['play_cursor_colour'])
            visible_col_nums = range(
                    self._first_col,
                    min(COLUMNS_MAX, self._first_col + self._visible_cols))
            for col_num in visible_col_nums:
                col_x_offset = self._get_col_offset(col_num)
                tfm = QTransform().translate(
                        col_x_offset, self._playback_cursor_offset - self._px_offset)
                painter.setTransform(tfm)
                painter.drawLine(QPoint(0, 0), QPoint(self._col_width - 2, 0))

        # Draw edit cursor
        if self._sheet_mgr.get_edit_mode() or self._field_edit.is_editing():
            self._draw_edit_cursor(painter)

        # Draw selected area
        selection = self._ui_model.get_selection()
        if selection.has_rect_area():
            self._draw_selected_area_rect(
                    painter, selection, draw_col_start, draw_col_stop)

        if not self.isEnabled():
            painter.setTransform(QTransform())
            painter.fillRect(
                    0, 0, self.width(), self.height(), self._config['disabled_colour'])

        if pixmaps_created == 0:
            # Update was easy; predraw some likely upcoming pixmaps
            max_visible_cols = utils.get_max_visible_cols(self.width(), self._col_width)
            pixmap_count = min(max(1, max_visible_cols // 4), 10)
            for _ in range(pixmap_count):
                index_add = next(self._upcoming_test_start_index_add)
                for rel_col_index in range(draw_col_start, draw_col_stop):
                    col_count = draw_col_stop - draw_col_start
                    index = (self._first_col + rel_col_index + index_add) % col_count
                    if self._col_rends[index].predraw(self.height(), grid):
                        #print('1 column pixmap predrawn')
                        break
        else:
            pass
            #print('{} column pixmap{} created'.format(
            #    pixmaps_created, 's' if pixmaps_created != 1 else ''))

        new_trigger_count = self._trigger_cache.trigger_count()
        triggers_rendered = max(0, new_trigger_count - old_trigger_count)

        end = time.time()
        elapsed = end - start
        #memory_usage = (sum(cr.get_memory_usage() for cr in self._col_rends) +
        #        self._trigger_cache.get_memory_usage())
        #print('View updated in {:.2f} ms, cache size {:.2f} MB'.format(
        #    elapsed * 1000, memory_usage / float(2**20)))
        #print('{} trigger{} rendered'.format(
        #    triggers_rendered, '' if triggers_rendered == 1 else 's'))

    def focusInEvent(self, event):
        self._sheet_mgr.set_edit_mode(True)

    def focusOutEvent(self, event):
        module = self._ui_model.get_module()
        allow_signals = (
                self._visibility_mgr.is_show_allowed() and
                not module.is_saving() and
                not module.is_importing_audio_unit())

        if allow_signals:
            self._sheet_mgr.set_edit_mode(False)
            self._sheet_mgr.set_chord_mode(False)

        if event.reason() == Qt.OtherFocusReason:
            # The user (hopefully) clicked the Save button while focused here
            self._force_focus_on_enable = True

    def _check_fix_focus(self):
        if self._force_focus_on_enable:
            self._force_focus_on_enable = False
            self.setFocus()

    def changeEvent(self, event):
        if (self._force_focus_on_enable and
                event.type() == QEvent.EnabledChange and
                self.isEnabled()):
            self.checkFixFocus.emit()
        event.ignore()

    def mousePressEvent(self, event):
        if event.buttons() == Qt.LeftButton:
            self._mouse_selection_snapped_out = False

            selection = self._ui_model.get_selection()
            if selection.has_area_start():
                selection.clear_area()

            new_location = self._get_selected_location(event.x(), event.y())
            if new_location:
                self._edit_px_offset = self._px_offset
                selection.set_location(new_location)

    def mouseMoveEvent(self, event):
        if event.buttons() == Qt.LeftButton:
            selection = self._ui_model.get_selection()
            orig_location = selection.get_location()
            selection.try_set_area_start(orig_location)

            new_location = self._get_selected_location(event.x(), event.y())
            if new_location:
                # Clamp our new location to the original pattern instance
                orig_track = orig_location.get_track()
                orig_system = orig_location.get_system()
                new_track = new_location.get_track()
                new_system = new_location.get_system()
                if (new_track, new_system) < (orig_track, orig_system):
                    new_location = TriggerPosition(
                            orig_track,
                            orig_system,
                            new_location.get_col_num(),
                            tstamp.Tstamp(0),
                            0)
                elif (new_track, new_system) > (orig_track, orig_system):
                    module = self._ui_model.get_module()
                    album = module.get_album()
                    song = album.get_song_by_track(orig_track)
                    pinst = song.get_pattern_instance(orig_system)
                    pattern = pinst.get_pattern()
                    new_location = TriggerPosition(
                            orig_track,
                            orig_system,
                            new_location.get_col_num(),
                            pattern.get_length(),
                            0)

                area_start = selection.get_area_start()
                area_start_y = utils.get_px_from_tstamp(
                        area_start.get_row_ts(), self._px_per_beat)
                new_y = utils.get_px_from_tstamp(
                        new_location.get_row_ts(), self._px_per_beat)
                y_dist = abs(new_y - area_start_y)

                if (self._sheet_mgr.is_grid_enabled() or
                        self._mouse_selection_snapped_out or
                        area_start.get_col_num() != new_location.get_col_num() or
                        y_dist >= self._config['tr_height']):
                    self._mouse_selection_snapped_out = True
                    selection.set_location(new_location)
                    selection.set_area_stop(new_location)


