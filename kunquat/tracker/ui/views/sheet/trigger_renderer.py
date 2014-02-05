# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2014
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from PyQt4.QtCore import *
from PyQt4.QtGui import *

from config import *


class TriggerRenderer():

    def __init__(self, config, trigger):
        assert trigger
        self._config = config
        self._trigger = trigger

    def get_trigger_width(self, pdev): # TODO: note names
        evtype = self._trigger.get_type()
        expr = self._trigger.get_argument()

        # Padding
        total_padding = self._config['trigger']['padding'] * 2
        if expr != None:
            # Space between type and expression
            total_padding += self._config['trigger']['padding']

        # Text
        metrics = self._config['font_metrics']
        self._baseline_offset = metrics.tightBoundingRect('A').height()
        evtype_width = metrics.boundingRect(evtype).width()
        expr_width = 0
        if expr != None:
            expr_width = metrics.boundingRect(expr).width()

        # Drawing parameters
        self._evtype_offset = self._config['trigger']['padding']
        self._expr_offset = (self._evtype_offset + evtype_width +
                self._config['trigger']['padding'])
        self._width = total_padding + evtype_width + expr_width

        return self._width

    def draw_trigger(self, painter, include_line=True, select=None):
        evtype = self._trigger.get_type()
        expr = self._trigger.get_argument()

        # TODO: select colour based on event type
        evtype_fg_colour = self._config['trigger']['default_colour']

        # Draw type
        painter.save()
        if select == 'type':
            painter.setBackgroundMode(Qt.OpaqueMode)
            painter.setBackground(QBrush(evtype_fg_colour))
            painter.setPen(self._config['bg_colour'])
        else:
            painter.setPen(evtype_fg_colour)

        painter.drawText(QPoint(self._evtype_offset, self._baseline_offset), evtype)

        painter.restore()

        if expr != None:
            # Draw expression
            painter.save()
            if select == 'expr':
                painter.setBackgroundMode(Qt.OpaqueMode)
                painter.setBackground(QBrush(evtype_fg_colour))
                painter.setPen(self._config['bg_colour'])
            else:
                painter.setPen(evtype_fg_colour)

            painter.drawText(QPoint(self._expr_offset, self._baseline_offset), expr)

            painter.restore()

        # Draw line only if not obscured by cursor
        if include_line:
            painter.save()
            painter.setPen(evtype_fg_colour)
            painter.drawLine(QPoint(0, 0), QPoint(self._width - 2, 0))
            painter.restore()


