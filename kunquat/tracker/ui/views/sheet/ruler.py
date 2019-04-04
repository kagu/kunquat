# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2013-2019
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

import math
import time

from kunquat.tracker.ui.qt import *

import kunquat.tracker.ui.model.tstamp as tstamp
from kunquat.tracker.ui.views.updater import Updater
from .config import *
from . import utils


class Ruler(QWidget, Updater):

    heightChanged = Signal(name='heightChanged')

    def __init__(self, is_grid_ruler=False):
        super().__init__()
        self._is_grid_ruler = is_grid_ruler

        self._width = 16

        self._is_playback_cursor_visible = False
        self._playback_cursor_offset = None

        self._playback_marker_offset = None

        self._pinsts = []

        self._lengths = []
        self._px_offset = 0
        self._px_per_beat = None
        self._cache = RulerCache()
        self._inactive_cache = RulerCache()
        self._inactive_cache.set_inactive()

        self._heights = []
        self._start_heights = []

        self.setAutoFillBackground(False)
        self.setAttribute(Qt.WA_OpaquePaintEvent)
        self.setAttribute(Qt.WA_NoSystemBackground)

    def set_config(self, config):
        self._config = config

        fm = QFontMetrics(self._config['font'], self)
        self._config['font_metrics'] = fm
        num_space = fm.tightBoundingRect('00.000')
        self._width = (num_space.width() +
                self._config['line_len_long'] +
                self._config['num_padding_left'] +
                self._config['num_padding_right'])

        self._cache.set_config(config)
        self._cache.set_width(self._width)
        self._cache.set_num_height(num_space.height())

        self._inactive_cache.set_config(config)
        self._inactive_cache.set_width(self._width)
        self._inactive_cache.set_num_height(num_space.height())

        self.update()

    def _on_setup(self):
        if not self._is_grid_ruler:
            self.register_action('signal_module', self._update_all_patterns)
            self.register_action('signal_order_list', self._update_all_patterns)
            self.register_action('signal_pattern_length', self._update_all_patterns)
            self.register_action('signal_playback_cursor', self._update_playback_cursor)
            self.register_action('signal_playback_marker', self._update_playback_marker)

        self.register_action('signal_selection', self.update)

    def get_total_height(self):
        return sum(self._heights)

    def _update_playback_cursor(self):
        if self._is_grid_ruler:
            return

        playback_mgr = self._ui_model.get_playback_manager()
        was_playback_cursor_visible = self._is_playback_cursor_visible

        prev_offset = self._playback_cursor_offset

        self._is_playback_cursor_visible = False
        self._playback_cursor_offset = None

        if playback_mgr.is_playback_active():
            track_num, system_num, row_ts = playback_mgr.get_playback_position()
            if track_num < 0 or system_num < 0:
                ploc = utils.get_current_playback_pattern_location(self._ui_model)
                if ploc:
                    track_num, system_num = ploc

            cur_pinst = None

            try:
                module = self._ui_model.get_module()
                album = module.get_album()
                song = album.get_song_by_track(track_num)
                cur_pinst = song.get_pattern_instance(system_num)
            except (IndexError, AttributeError):
                pass

            if cur_pinst != None:
                for pinst, start_height in zip(self._pinsts, self._start_heights):
                    if cur_pinst == pinst:
                        start_px = start_height - self._px_offset
                        location_from_start_px = (
                                (row_ts.beats * tstamp.BEAT + row_ts.rem) *
                                self._px_per_beat) // tstamp.BEAT
                        self._playback_cursor_offset = (
                                location_from_start_px + start_px + self._px_offset)

        if self._playback_cursor_offset != None:
            if 0 <= (self._playback_cursor_offset - self._px_offset) < self.height():
                self._is_playback_cursor_visible = True
                if prev_offset != self._playback_cursor_offset:
                    self.update()

        if (not self._is_playback_cursor_visible) and was_playback_cursor_visible:
            self.update()

    def _update_playback_marker(self):
        playback_mgr = self._ui_model.get_playback_manager()

        self._playback_marker_offset = None

        marker = playback_mgr.get_playback_marker()
        if marker:
            track_num, system_num, row_ts = marker

            cur_pinst = None
            try:
                module = self._ui_model.get_module()
                album = module.get_album()
                song = album.get_song_by_track(track_num)
                cur_pinst = song.get_pattern_instance(system_num)
            except (IndexError, AttributeError):
                pass

            if cur_pinst != None:
                for pinst, start_height in zip(self._pinsts, self._start_heights):
                    if cur_pinst == pinst:
                        start_px = start_height - self._px_offset
                        location_from_start_px = (
                                (row_ts.beats * tstamp.BEAT + row_ts.rem) *
                                self._px_per_beat) // tstamp.BEAT
                        self._playback_marker_offset = (
                                location_from_start_px + start_px + self._px_offset)

        self.update()

    def update_grid_pattern(self):
        assert self._is_grid_ruler

        self._lengths = []

        grid_mgr = self._ui_model.get_grid_manager()
        gp_id = grid_mgr.get_selected_grid_pattern_id()
        if gp_id != None:
            gp = grid_mgr.get_grid_pattern(gp_id)
            gp_length = gp.get_length()
            self._lengths = [gp_length]

        self._set_pattern_heights()
        self.update()

    def _update_all_patterns(self):
        self._pinsts = utils.get_all_pattern_instances(self._ui_model)
        self.set_patterns(pinst.get_pattern() for pinst in self._pinsts)
        self.update()

    def set_px_per_beat(self, px_per_beat):
        changed = self._px_per_beat != px_per_beat
        self._px_per_beat = px_per_beat
        self._cache.set_px_per_beat(px_per_beat)
        self._inactive_cache.set_px_per_beat(px_per_beat)
        if self._playback_marker_offset != None:
            self._update_playback_marker()
            self.update()
        if changed:
            self._set_pattern_heights()
            self._update_playback_cursor()
            self.update()

    def set_patterns(self, patterns):
        self._lengths = [p.get_length() for p in patterns]
        self._set_pattern_heights()

    def _set_pattern_heights(self):
        self._heights = utils.get_pat_heights(self._lengths, self._px_per_beat)
        self._start_heights = utils.get_pat_start_heights(self._heights)
        self.heightChanged.emit()

    def set_px_offset(self, offset):
        changed = offset != self._px_offset
        self._px_offset = offset
        if changed:
            self._set_pattern_heights()
            self._update_playback_cursor()
            self.update()

    def _get_final_colour(self, colour, inactive):
        if inactive:
            dim_factor = self._config['inactive_dim']
            new_colour = QColor(colour)
            new_colour.setRed(colour.red() * dim_factor)
            new_colour.setGreen(colour.green() * dim_factor)
            new_colour.setBlue(colour.blue() * dim_factor)
            return new_colour
        return colour

    def paintEvent(self, ev):
        start = time.time()

        painter = QPainter(self)

        # Render rulers of visible patterns
        first_index = utils.get_first_visible_pat_index(
                self._px_offset,
                self._start_heights)

        rel_end_height = 0 # empty song

        # Get pattern index that contains the active cursor
        playback_mgr = self._ui_model.get_playback_manager()
        if (playback_mgr.follow_playback_cursor() and not playback_mgr.is_recording()):
            active_pattern_index = utils.get_current_playback_pattern_index(
                    self._ui_model)
        else:
            selection = self._ui_model.get_selection()
            location = selection.get_location()
            if location:
                active_pattern_index = utils.get_pattern_index_at_location(
                        self._ui_model, location.get_track(), location.get_system())
            else:
                active_pattern_index = None

        for pi in range(first_index, len(self._heights)):
            if self._start_heights[pi] > self._px_offset + self.height():
                break

            # Current pattern offset and height
            rel_start_height = self._start_heights[pi] - self._px_offset
            rel_end_height = rel_start_height + self._heights[pi]
            cur_offset = max(0, -rel_start_height)

            # Choose cache based on whether this pattern contains the edit cursor
            cache = self._cache if (pi == active_pattern_index) else self._inactive_cache

            # Draw pixmaps
            canvas_y = max(0, rel_start_height)
            for (src_rect, pixmap) in cache.iter_pixmaps(
                    cur_offset, min(rel_end_height, self.height()) - canvas_y):
                dest_rect = QRect(0, canvas_y, self.width(), src_rect.height())
                if pi - 1 == active_pattern_index:
                    # Make sure the active bottom border remains visible
                    src_rect.setTop(src_rect.top() + 1)
                    dest_rect.setTop(dest_rect.top() + 1)
                painter.drawPixmap(dest_rect, pixmap, src_rect)
                canvas_y += src_rect.height()

            # Draw bottom border
            if rel_end_height <= self.height():
                pen = QPen(self._get_final_colour(
                    self._config['fg_colour'], pi != active_pattern_index))
                pen.setWidthF(self._config['line_width'])
                painter.setPen(pen)
                painter.drawLine(
                        QPoint(0, rel_end_height - 1),
                        QPoint(self._width - 1, rel_end_height - 1))
        else:
            # Fill trailing blank
            painter.setBackground(self._config['canvas_bg_colour'])
            if rel_end_height < self.height():
                painter.eraseRect(
                        QRect(
                            0, rel_end_height,
                            self._width, self.height() - rel_end_height)
                        )

        if self._playback_marker_offset != None:
            painter.setPen(self._config['play_marker_colour'])
            offset = self._playback_marker_offset - self._px_offset
            painter.drawLine(0, offset, self._width, offset)

        if self._playback_cursor_offset != None:
            painter.setPen(self._config['play_cursor_colour'])
            offset = self._playback_cursor_offset - self._px_offset
            painter.drawLine(0, offset, self._width, offset)

        if not self.isEnabled():
            painter.fillRect(
                    0, 0, self.width(), self.height(), self._config['disabled_colour'])

        end = time.time()
        elapsed = end - start
        #print('Ruler updated in {:.2f} ms'.format(elapsed * 1000))

    def resizeEvent(self, event):
        self._cache.set_width(event.size().width())
        self._inactive_cache.set_width(event.size().width())
        self._update_playback_cursor()

    def sizeHint(self):
        return QSize(self._width, 128)


class RulerCache():

    PIXMAP_HEIGHT = 256

    def __init__(self):
        self._width = 0
        self._px_per_beat = None
        self._pixmaps = {}
        self._inactive = False

    def set_inactive(self):
        self._inactive = True

    def set_config(self, config):
        self._config = config
        self._pixmaps = {}

    def set_width(self, width):
        if width != self._width:
            self._pixmaps = {}
        self._width = width

    def set_num_height(self, height):
        self._num_height = height

    def set_px_per_beat(self, px_per_beat):
        if px_per_beat != self._px_per_beat:
            self._pixmaps = {}
        self._px_per_beat = px_per_beat

    def iter_pixmaps(self, start_px, height_px):
        assert start_px >= 0
        assert height_px >= 0

        stop_px = start_px + height_px

        create_count = 0

        for i in utils.get_pixmap_indices(start_px, stop_px, RulerCache.PIXMAP_HEIGHT):
            if i not in self._pixmaps:
                self._pixmaps[i] = self._create_pixmap(i)
                create_count += 1

            rect = utils.get_pixmap_rect(
                    i,
                    start_px, stop_px,
                    self._width,
                    RulerCache.PIXMAP_HEIGHT)

            yield (rect, self._pixmaps[i])

        if create_count > 0:
            """
            print('{} ruler pixmap{} created'.format(
                create_count, 's' if create_count != 1 else ''))
            """

    def _get_final_colour(self, colour):
        if self._inactive:
            dim_factor = self._config['inactive_dim']
            new_colour = QColor(colour)
            new_colour.setRed(colour.red() * dim_factor)
            new_colour.setGreen(colour.green() * dim_factor)
            new_colour.setBlue(colour.blue() * dim_factor)
            return new_colour
        return colour

    def _create_pixmap(self, index):
        pixmap = QPixmap(self._width, RulerCache.PIXMAP_HEIGHT)

        cfg = self._config

        painter = QPainter(pixmap)

        # Background
        painter.setBackground(self._get_final_colour(cfg['bg_colour']))
        painter.eraseRect(QRect(0, 0, self._width, RulerCache.PIXMAP_HEIGHT))

        # Lines
        painter.save()

        line_width = cfg['line_width']
        line_pen = QPen(self._get_final_colour(cfg['fg_colour']))
        line_pen.setWidthF(line_width)
        painter.setPen(line_pen)
        painter.translate(0, -((line_width - 1) // 2))

        painter.drawLine(
                QPoint(self._width - line_width / 2, 0),
                QPoint(self._width - line_width / 2, RulerCache.PIXMAP_HEIGHT - 1))

        # Ruler lines
        slice_margin_ts = utils.get_tstamp_from_px(line_width / 2, self._px_per_beat)

        start_ts = tstamp.Tstamp(0, tstamp.BEAT *
                index * RulerCache.PIXMAP_HEIGHT // self._px_per_beat)
        stop_ts = tstamp.Tstamp(0, tstamp.BEAT *
                (index + 1) * RulerCache.PIXMAP_HEIGHT // self._px_per_beat)

        def draw_ruler_line(painter, y, line_pos, lines_per_beat):
            if line_pos[1] == 0:
                line_length = cfg['line_len_long'] if line_pos[0] != 0 else self._width
            else:
                line_length = cfg['line_len_short']
            painter.drawLine(
                    QPoint(self._width - 1 - line_length, y),
                    QPoint(self._width - 1, y))

        self._draw_markers(
                painter,
                start_ts - slice_margin_ts,
                stop_ts + slice_margin_ts,
                cfg['line_min_dist'],
                draw_ruler_line)

        painter.restore()

        # Beat numbers
        painter.setPen(self._get_final_colour(cfg['fg_colour']))

        num_extent = self._num_height // 2
        start_ts = tstamp.Tstamp(0, tstamp.BEAT *
                (index * RulerCache.PIXMAP_HEIGHT - num_extent) //
                self._px_per_beat)
        stop_ts = tstamp.Tstamp(0, tstamp.BEAT *
                ((index + 1) * RulerCache.PIXMAP_HEIGHT + num_extent) //
                self._px_per_beat)

        text_option = QTextOption(Qt.AlignRight | Qt.AlignVCenter)

        def draw_number(painter, y, num_pos, nums_per_beat):
            if num_pos == [0, 0]:
                return

            # Text
            num = num_pos[0] + num_pos[1] / float(nums_per_beat)
            numi = int(num)
            text = str(numi) if num == numi else str(round(num, 3))

            # Draw
            right_offset = cfg['line_len_long'] + cfg['num_padding_right']
            rect = QRectF(0, y - self._num_height,
                    self._width - right_offset, self._num_height + 3)
            painter.drawText(rect, text, text_option)

        painter.setFont(self._config['font'])
        self._draw_markers(
                painter,
                start_ts,
                stop_ts,
                cfg['num_min_dist'],
                draw_number)

        # Draw pixmap index for debugging
        #painter.drawText(QPoint(2, 12), str(index))

        return pixmap

    def _draw_markers(self, painter, start_ts, stop_ts, min_dist, draw_fn):
        cfg = self._config

        beat_div_base = 2

        if min_dist <= self._px_per_beat:
            markers_per_beat = self._px_per_beat // min_dist
            markers_per_beat = int(beat_div_base**math.floor(
                    math.log(markers_per_beat, beat_div_base)))

            # First visible marker in the first beat
            start_beat_frac = start_ts.rem / float(tstamp.BEAT)
            start_marker_in_beat = int(
                    math.ceil(start_beat_frac * markers_per_beat))

            # First non-visible marker in the last beat
            stop_beat_frac = stop_ts.rem / float(tstamp.BEAT)
            stop_marker_in_beat = int(
                    math.ceil(stop_beat_frac * markers_per_beat))

            def normalise_marker_pos(pos):
                excess = pos[1] // markers_per_beat
                pos[0] += excess
                pos[1] -= excess * markers_per_beat
                assert pos[1] >= 0
                assert pos[1] < markers_per_beat

            # Loop boundaries
            marker_pos = [start_ts.beats, start_marker_in_beat]
            normalise_marker_pos(marker_pos)

            stop_pos = [stop_ts.beats, stop_marker_in_beat]
            normalise_marker_pos(stop_pos)

            # Draw markers
            while marker_pos < stop_pos:
                ts = tstamp.Tstamp(marker_pos[0] +
                        marker_pos[1] / float(markers_per_beat))
                y = float(ts - start_ts) * self._px_per_beat

                draw_fn(painter, y, marker_pos, markers_per_beat)

                # Next marker
                marker_pos[1] += 1
                normalise_marker_pos(marker_pos)

        else:
            # Zoomed far out -- skipping whole beats
            beats_per_marker = (min_dist +
                    self._px_per_beat - 1) // self._px_per_beat
            beats_per_marker = int(beat_div_base**math.ceil(
                    math.log(beats_per_marker, beat_div_base)))

            # First beat with a visible marker
            start_beat = (start_ts - tstamp.Tstamp(0, 1)).beats + 1
            start_marker_in_beat = beats_per_marker * int(
                    math.ceil(start_beat / float(beats_per_marker)))

            # First non-visible beat
            stop_beat = (stop_ts - tstamp.Tstamp(0, 1)).beats + 1

            # Draw markers
            for marker_pos in range(
                    start_marker_in_beat, stop_beat, beats_per_marker):
                y = float(marker_pos - start_ts) * self._px_per_beat
                draw_fn(painter, y, [marker_pos, 0], 1)


