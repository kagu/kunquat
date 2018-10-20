# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2015-2018
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from kunquat.tracker.ui.qt import *


def get_abs_window_size(width_norm, height_norm):
    screen_rect = QApplication.desktop().screenGeometry()
    return QSize(
            int(screen_rect.width() * width_norm),
            int(screen_rect.height() * height_norm))

def get_default_font():
    screen = QApplication.primaryScreen()
    dpi = screen.physicalDotsPerInch()
    # 96 DPI is probably just the default setting that does not reflect reality
    size = 9 if (dpi == 96) else 7
    return QFont(QFont().defaultFamily(), size)

def lerp_val(v1, v2, t):
    assert 0 <= t <= 1, 'lerp value {} is not within valid range [0, 1]'.format(t)
    return v1 + (v2 - v1) * t

def lerp_colour(c1, c2, t):
    assert 0 <= t <= 1, 'lerp value {} is not within valid range [0, 1]'.format(t)
    return QColor(
            lerp_val(c1.red(), c2.red(), t),
            lerp_val(c1.green(), c2.green(), t),
            lerp_val(c1.blue(), c2.blue(), t))

def get_colour_from_str(s):
    if len(s) == 4:
        cs = [s[1], s[2], s[3]]
        cs = [c + c for c in cs]
    elif len(s) == 7:
        cs = [s[1:3], s[3:5], s[5:7]]
    else:
        assert False
    colour = [int(c, 16) for c in cs]
    return QColor(colour[0], colour[1], colour[2])

def get_str_from_colour(colour):
    components = [colour.red(), colour.green(), colour.blue()]
    cs = ['{:02x}'.format(c) for c in components]
    s = '#' + ''.join(cs)
    assert len(s) == 7
    return s

def get_glyph_rel_width(font, widget_cls, ref_str):
    fm = QFontMetricsF(font, widget_cls())
    rect = fm.tightBoundingRect(ref_str)
    return rect.width() / rect.height()

def set_glyph_rel_width(font, widget_cls, ref_str, rel_width):
    cur_rel_width = get_glyph_rel_width(font, widget_cls, ref_str)
    stretch = int(round(100 * rel_width / cur_rel_width))
    font.setStretch(stretch)


