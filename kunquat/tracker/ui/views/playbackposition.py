# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2016-2018
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from itertools import count
import math
import time

from kunquat.tracker.ui.qt import *

from kunquat.kunquat.limits import *
import kunquat.tracker.ui.model.tstamp as tstamp
from . import utils


_REF_NUM_STR = '23456789' * 8


class PlaybackPosition(QWidget):

    _NUM_FONT = QFont(QFont().defaultFamily(), 16, QFont.Bold)
    utils.set_glyph_rel_width(_NUM_FONT, QWidget, _REF_NUM_STR, 51.875)

    _SUB_FONT = QFont(QFont().defaultFamily(), 8, QFont.Bold)
    utils.set_glyph_rel_width(_SUB_FONT, QWidget, _REF_NUM_STR, 56)

    _REM_FONT = QFont(QFont().defaultFamily(), 12, QFont.Bold)
    utils.set_glyph_rel_width(_REM_FONT, QWidget, _REF_NUM_STR, 53.25)

    _TITLE_FONT = QFont(QFont().defaultFamily(), 5, QFont.Bold)

    _DEFAULT_CONFIG = {
        'padding_x'      : 9,
        'padding_y'      : 0,
        'spacing'        : 2,
        'icon_width'     : 19,
        'num_font'       : _NUM_FONT,
        'sub_font'       : _SUB_FONT,
        'rem_font'       : _REM_FONT,
        'title_font'     : _TITLE_FONT,
        'narrow_factor'  : 0.6,
        'track_digits'   : [1, len(str(TRACKS_MAX - 1))],
        'system_digits'  : [2, len(str(SYSTEMS_MAX - 1))],
        'pat_digits'     : [2, len(str(PATTERNS_MAX - 1))],
        'pat_inst_digits': [1, len(str(PAT_INSTANCES_MAX - 1))],
        'ts_beat_digits' : [2, 3],
        'ts_rem_digits'  : [1, 1],
        'bg_colour'      : QColor(0, 0, 0),
        'fg_colour'      : QColor(0x66, 0xdd, 0x66),
        'stopped_colour' : QColor(0x55, 0x55, 0x55),
        'play_colour'    : QColor(0x66, 0xdd, 0x66),
        'record_colour'  : QColor(0xdd, 0x44, 0x33),
        'infinite_colour': QColor(0xff, 0xdd, 0x55),
        'title_colour'   : QColor(0x77, 0x77, 0x77),
    }

    _STOPPED = 'stopped'
    _PLAYING = 'playing'
    _RECORDING = 'recording'

    def __init__(self):
        super().__init__()
        self._ui_model = None

        self._widths = None
        self._min_height = 0

        self._state = (self._STOPPED, False)

        self._state_icons = {}
        self._inf_image = None

        self._glyphs = {}
        self._glyph_sizes = {}

        self._titles = {}

        self._baseline_offsets = {}

        self._config = None

        self.setAutoFillBackground(False)
        self.setAttribute(Qt.WA_OpaquePaintEvent)
        self.setAttribute(Qt.WA_NoSystemBackground)

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model
        self._updater = ui_model.get_updater()
        self._updater.register_updater(self._perform_updates)

        self._update_style()

    def unregister_updaters(self):
        self._updater.unregister_updater(self._perform_updates)

    def _update_style(self):
        style_mgr = self._ui_model.get_style_manager()

        num_font = utils.get_scaled_font(style_mgr, 1.6, QFont.Bold)
        utils.set_glyph_rel_width(num_font, QWidget, _REF_NUM_STR, 51.875)
        sub_font = utils.get_scaled_font(style_mgr, 0.9, QFont.Bold)
        utils.set_glyph_rel_width(sub_font, QWidget, _REF_NUM_STR, 56)
        rem_font = utils.get_scaled_font(style_mgr, 1.2, QFont.Bold)
        utils.set_glyph_rel_width(rem_font, QWidget, _REF_NUM_STR, 53.25)
        title_font = utils.get_scaled_font(style_mgr, 0.5, QFont.Bold)

        config = {
            'padding_x' : style_mgr.get_scaled_size(1),
            'padding_y' : 0,
            'spacing'   : style_mgr.get_scaled_size(0.2),
            'icon_width': style_mgr.get_scaled_size(1.6),
            'num_font'  : num_font,
            'sub_font'  : sub_font,
            'rem_font'  : rem_font,
            'title_font': title_font,

            'bg_colour':
                QColor(style_mgr.get_style_param('position_bg_colour')),
            'fg_colour':
                QColor(style_mgr.get_style_param('position_fg_colour')),
            'stopped_colour':
                QColor(style_mgr.get_style_param('position_stopped_colour')),
            'play_colour':
                QColor(style_mgr.get_style_param('position_play_colour')),
            'record_colour':
                QColor(style_mgr.get_style_param('position_record_colour')),
            'infinite_colour':
                QColor(style_mgr.get_style_param('position_infinite_colour')),
            'title_colour':
                QColor(style_mgr.get_style_param('position_title_colour')),
        }
        self._set_config(config)
        self.update()

    def _set_config(self, config):
        self._config = self._DEFAULT_CONFIG.copy()
        self._config.update(config)

        self._state_icons = {}
        self._inf_image = self._get_infinity_image()

        self._glyphs = {}
        self._draw_glyphs()
        self._set_dimensions()

        self._draw_titles()

    def _draw_glyphs(self):
        font_names = ('num_font', 'sub_font', 'rem_font')

        for font_name in font_names:
            font = self._config[font_name]
            fm = QFontMetrics(font, self)

            chars = '.-0123456789'

            normal_width = max(fm.tightBoundingRect(c).width() + 1 for c in chars)
            height = fm.boundingRect('0').height()

            text_option = QTextOption(Qt.AlignCenter)

            for width_factor in (1, self._config['narrow_factor']):
                cur_glyphs = {}
                width = math.ceil(width_factor * normal_width)
                rect = QRectF(0, 0, normal_width, height)
                for c in chars:
                    pixmap = QPixmap(width - 1, height)
                    pixmap.fill(self._config['bg_colour'])
                    painter = QPainter(pixmap)
                    painter.scale(width_factor, 1)
                    painter.setFont(self._config[font_name])
                    painter.setPen(self._config['fg_colour'])
                    painter.drawText(rect, c, text_option)
                    cur_glyphs[c] = pixmap
                self._glyphs[(font_name, width_factor)] = cur_glyphs

                self._glyph_sizes[(font_name, width_factor)] = (width - 1, height)

    def _draw_titles(self):
        # Render pixmaps at larger scale for more accurate width adjustment
        scale_mult = 4

        font = QFont(self._config['title_font'])
        font.setPointSizeF(font.pointSizeF() * scale_mult)
        utils.set_glyph_rel_width(font, QWidget, 'TRACKSYSTEMPATTERNROW', 20)

        fm = QFontMetrics(font, self)
        text_option = QTextOption(Qt.AlignLeft | Qt.AlignTop)

        bounding_rects = {}

        titles = ('track', 'system', 'pattern', 'row')
        for title in titles:
            vis_text = title.upper()
            bounding_rects[title] = fm.tightBoundingRect(vis_text)

        max_height = max(r.height() for r in bounding_rects.values())
        target_height = int(math.ceil(max_height / scale_mult)) + 2

        for title in titles:
            vis_text = title.upper()
            rect = bounding_rects[title]

            pixmap = QPixmap(
                    rect.width() + scale_mult * 2, max_height + scale_mult * 2)
            pixmap.fill(self._config['bg_colour'])
            painter = QPainter(pixmap)
            painter.setFont(font)
            painter.setPen(self._config['title_colour'])
            painter.drawText(pixmap.rect(), Qt.AlignCenter, vis_text)
            painter.end()

            self._titles[title] = pixmap.scaledToHeight(
                    target_height, Qt.SmoothTransformation)

    def _set_dimensions(self):
        default_fm = QFontMetrics(self._config['num_font'], self)

        padding_x = self._config['padding_x']
        spacing = self._config['spacing']
        track_width = self._get_field_width(self._config['track_digits'], 'num_font')
        system_width = self._get_field_width(self._config['system_digits'], 'num_font')
        pat_width = self._get_field_width(self._config['pat_digits'], 'num_font')
        inst_width = self._get_field_width(self._config['pat_inst_digits'], 'sub_font')
        beat_width = self._get_field_width(self._config['ts_beat_digits'], 'num_font')
        dot_width = default_fm.boundingRect('.').width()
        rem_width = self._get_field_width(self._config['ts_rem_digits'], 'rem_font')

        self._widths = [
            padding_x,
            self._config['icon_width'],
            spacing * 2,
            track_width,
            spacing * 2,
            system_width,
            0,
            pat_width,
            inst_width,
            spacing * 4,
            beat_width,
            dot_width,
            rem_width,
            padding_x]

        padding_y = self._config['padding_y']
        self._min_height = default_fm.boundingRect('Ag').height() + padding_y * 2

    def _get_field_width(self, digit_counts, font_name):
        min_digits, max_digits = digit_counts
        normal_width, _ = self._glyph_sizes[(font_name, 1)]
        narrow_width, _ = self._glyph_sizes[(font_name, self._config['narrow_factor'])]
        min_width = min_digits * normal_width
        scaled_max_width = max_digits * narrow_width
        return max(min_width, scaled_max_width)

    def _get_current_state(self):
        playback_mgr = self._ui_model.get_playback_manager()
        is_playing = playback_mgr.is_playback_active()
        is_infinite = playback_mgr.get_infinite_mode()
        is_recording = playback_mgr.is_recording()

        playback_state = 'playing' if is_playing else 'stopped'
        if is_recording:
            playback_state = 'recording'

        return (playback_state, is_infinite)

    def _perform_updates(self, signals):
        state = self._get_current_state()
        if (state != self._state) or (state[0] != self._STOPPED):
            self._state = state
            self.update()

        if 'signal_style_changed' in signals:
            self._update_style()

    def minimumSizeHint(self):
        return QSize(sum(self._widths), self._min_height)

    def _get_icon_rect(self, width_norm):
        icon_width = self._config['icon_width']
        side_length = icon_width * width_norm
        offset = (icon_width - side_length) * 0.5
        rect = QRectF(offset, offset, side_length, side_length)
        return rect

    def _draw_stop_icon_shape(self, painter, colour):
        painter.setPen(Qt.NoPen)
        painter.setBrush(colour)
        painter.setRenderHint(QPainter.Antialiasing)

        rect = self._get_icon_rect(0.85)

        painter.drawRect(rect)

    def _draw_play_icon_shape(self, painter, colour):
        painter.setPen(Qt.NoPen)
        painter.setBrush(colour)
        painter.setRenderHint(QPainter.Antialiasing)

        rect = self._get_icon_rect(0.85)

        middle_y = rect.y() + (rect.height() / 2.0)
        shape = QPolygonF([
            QPointF(rect.x(), rect.y()),
            QPointF(rect.x(), rect.y() + rect.height()),
            QPointF(rect.x() + rect.width(), rect.y() + (rect.height() * 0.5))])

        painter.drawPolygon(shape)

    def _draw_record_icon_shape(self, painter, colour):
        painter.setPen(Qt.NoPen)
        painter.setBrush(colour)
        painter.setRenderHint(QPainter.Antialiasing)

        rect = self._get_icon_rect(0.85)

        painter.drawEllipse(rect)

    def _get_infinity_image(self):
        icon_width = self._config['icon_width']

        image = QImage(icon_width, icon_width, QImage.Format_ARGB32_Premultiplied)
        image.fill(0)
        painter = QPainter(image)
        painter.setPen(Qt.NoPen)
        painter.setRenderHint(QPainter.Antialiasing)

        centre_y = icon_width * 0.5
        height_norm = 0.4

        def draw_drop_shape(round_x_norm, sharp_x_norm):
            round_x = round_x_norm * icon_width
            sharp_x = sharp_x_norm * icon_width
            width = abs(round_x - sharp_x)
            height = width * height_norm

            points = []
            point_count = 15
            for i in range(point_count):
                norm_t = i / float(point_count - 1)
                if norm_t < 0.5:
                    t = utils.lerp_val(math.pi, math.pi * 1.5, norm_t * 2)
                else:
                    t = utils.lerp_val(math.pi * 4.5, math.pi * 5, (norm_t - 0.5) * 2)

                norm_x = min(max(0, math.cos(t) + 1), 1)
                norm_y = math.sin(t * 2)
                x = utils.lerp_val(round_x, sharp_x, norm_x)
                y = centre_y + (norm_y * height)

                points.append(QPointF(x, y))

            shape = QPolygonF(points)
            painter.drawPolygon(shape)

        # Outline
        painter.setBrush(self._config['bg_colour'])
        draw_drop_shape(-0.03, 0.68)
        draw_drop_shape(1.03, 0.32)

        # Coloured fill
        painter.setBrush(self._config['infinite_colour'])
        draw_drop_shape(0, 0.65)
        draw_drop_shape(1, 0.35)

        # Hole of the coloured fill
        painter.setBrush(self._config['bg_colour'])
        draw_drop_shape(0.1, 0.45)
        draw_drop_shape(0.9, 0.55)

        # Transparent hole
        painter.setBrush(QColor(0xff, 0xff, 0xff))
        painter.setCompositionMode(QPainter.CompositionMode_DestinationOut)
        draw_drop_shape(0.13, 0.42)
        draw_drop_shape(0.87, 0.58)

        painter.end()

        return image

    def _draw_state_icon(self, painter):
        if self._state not in self._state_icons:
            pixmap = QPixmap(self._config['icon_width'], self._config['icon_width'])
            pixmap.fill(self._config['bg_colour'])
            img_painter = QPainter(pixmap)

            playback_state, is_infinite = self._state
            if playback_state == self._STOPPED:
                self._draw_stop_icon_shape(img_painter, self._config['stopped_colour'])
            elif playback_state == self._PLAYING:
                self._draw_play_icon_shape(img_painter, self._config['play_colour'])
            elif playback_state == self._RECORDING:
                self._draw_record_icon_shape(img_painter, self._config['record_colour'])

            if is_infinite:
                assert self._inf_image
                img_painter.drawImage(0, 0, self._inf_image)

            self._state_icons[self._state] = pixmap

        icon = self._state_icons[self._state]
        painter.drawPixmap(0, -icon.height() // 2, icon)

    def _get_baseline_offset(self, font):
        key = font.pointSize()
        if key not in self._baseline_offsets:
            fm = QFontMetrics(font)
            self._baseline_offsets[key] = fm.tightBoundingRect('0').height()

        return self._baseline_offsets[key]

    def _get_num_width(self, num_str, digit_counts, font_name):
        min_digits, max_digits = digit_counts
        char_count = len(num_str)

        width_factor = 1 if char_count <= min_digits else self._config['narrow_factor']
        glyphs = self._glyphs[(font_name, width_factor)]
        assert glyphs
        width, _ = self._glyph_sizes[(font_name, width_factor)]

        total_width = width * char_count
        return total_width

    def _draw_str(
            self,
            painter,
            num_str,
            digit_counts,
            font_name,
            alignment=Qt.AlignCenter):
        min_digits, max_digits = digit_counts

        painter.save()

        char_count = len(num_str)
        width_factor = 1 if char_count <= min_digits else self._config['narrow_factor']

        glyphs = self._glyphs[(font_name, width_factor)]
        assert glyphs
        width, height = self._glyph_sizes[(font_name, width_factor)]

        total_width = width * char_count

        rect = painter.clipBoundingRect()
        if alignment == Qt.AlignCenter:
            start_x = (rect.width() - total_width) * 0.5
        elif alignment == Qt.AlignLeft:
            start_x = 0
        elif alignment == Qt.AlignRight:
            start_x = rect.width() - total_width
        else:
            assert False, 'Unexpected alignment: {}'.format(alignment)

        x = start_x
        y = -height // 2

        if font_name != 'num_font':
            _, default_height = self._glyph_sizes[('num_font', width_factor)]
            y += (default_height - height) // 2 - 1

        for c in num_str:
            glyph = glyphs[c]
            painter.drawPixmap(x, y, glyph)
            x += width

        painter.restore()

    def paintEvent(self, event):
        start = time.time()

        style_mgr = self._ui_model.get_style_manager()

        painter = QPainter(self)
        painter.setBackground(self._config['bg_colour'])
        painter.eraseRect(0, 0, self.width(), self.height())

        painter.translate(QPoint(0, self.height() // 2))

        width_index = count()
        def shift_x():
            cur_width_index = next(width_index)
            painter.translate(QPoint(self._widths[cur_width_index], 0))
            cur_width = self._widths[cur_width_index + 1]
            painter.setClipRect(QRectF(
                0, -self.height() * 0.5, cur_width, self.height()))

        title_y = self.height() * 0.3

        # Get position information
        track_num = -1
        system_num = -1
        row_ts = tstamp.Tstamp()
        pat_num = -1
        inst_num = -1

        playback_mgr = self._ui_model.get_playback_manager()
        if playback_mgr.is_playback_active():
            track_num, system_num, row_ts = playback_mgr.get_playback_position()

            pinst = playback_mgr.get_playback_pattern()
            album = self._ui_model.get_module().get_album()
            if pinst:
                pat_num, inst_num = pinst
            elif album.get_existence():
                song = album.get_song_by_track(track_num)
                if song.get_existence():
                    pinst = song.get_pattern_instance(system_num)
                    pat_num = pinst.get_pattern_num()
                    inst_num = pinst.get_instance_num()
            assert (pat_num == -1) == (inst_num == -1)

        # State icon
        shift_x()
        self._draw_state_icon(painter)

        # Track number
        shift_x()
        shift_x()
        self._draw_str(
                painter,
                str(track_num) if track_num >= 0 else '-',
                self._config['track_digits'],
                'num_font')

        painter.setClipping(False)
        painter.drawPixmap(
                -style_mgr.get_scaled_size(0.2, 0), title_y, self._titles['track'])

        # System number
        shift_x()
        shift_x()
        self._draw_str(
                painter,
                str(system_num) if system_num >= 0 else '-',
                self._config['system_digits'],
                'num_font')

        painter.setClipping(False)
        painter.drawPixmap(
                style_mgr.get_scaled_size(0.2, 0), title_y, self._titles['system'])

        # Pattern instance
        shift_x()
        shift_x()

        # Shift horizontally to align pattern + instance number properly
        pat_shift = 0
        if pat_num >= 0:
            pat_num_width = self._get_num_width(
                    str(pat_num), self._config['pat_digits'], 'num_font')
            pat_shift = 0.5 * (pat_num_width -
                    self._get_num_width('9', self._config['pat_digits'], 'num_font'))
            if inst_num >= 0:
                inst_num_width = self._get_num_width(
                        str(inst_num), self._config['pat_inst_digits'], 'sub_font')
                inst_def_width = self._get_num_width(
                        '9', self._config['pat_inst_digits'], 'sub_font')
                inst_shift = 0.5 * (inst_def_width - inst_num_width)
                pat_shift += inst_shift
        else:
            inst_def_width = self._get_num_width(
                    '9', self._config['pat_inst_digits'], 'sub_font')
            pat_shift = 0.4 * inst_def_width

        painter.save()
        painter.translate(pat_shift, 0)
        painter.setClipRegion(painter.clipRegion().translated(pat_shift, 0))
        self._draw_str(
                painter,
                str(pat_num) if pat_num >= 0 else '-',
                self._config['pat_digits'],
                'num_font',
                Qt.AlignRight)
        painter.restore()

        shift_x()

        if inst_num >= 0:
            painter.save()
            painter.translate(pat_shift, 0)
            painter.setClipRegion(painter.clipRegion().translated(pat_shift, 0))
            self._draw_str(
                    painter,
                    str(inst_num),
                    self._config['pat_inst_digits'],
                    'sub_font',
                    Qt.AlignLeft)
            painter.restore()

        painter.setClipping(False)
        painter.drawPixmap(
                -style_mgr.get_scaled_size(1.9, 0), title_y, self._titles['pattern'])

        # Timestamp
        beats, rem = row_ts
        rem_norm = int(
                float(rem / float(tstamp.BEAT)) * (10**self._config['ts_rem_digits'][0]))

        shift_x()
        shift_x()
        self._draw_str(
                painter,
                str(beats),
                self._config['ts_beat_digits'],
                'num_font',
                Qt.AlignRight)

        shift_x()
        self._draw_str(
                painter,
                '.',
                [1, 1],
                'num_font')

        shift_x()
        self._draw_str(
                painter,
                str(rem_norm),
                self._config['ts_rem_digits'],
                'rem_font',
                Qt.AlignLeft)

        painter.setClipping(False)
        painter.drawPixmap(
                -style_mgr.get_scaled_size(1.2, 0), title_y, self._titles['row'])

        end = time.time()
        elapsed = end - start
        #print('Playback position view updated in {:.2f} ms'.format(elapsed * 1000))


