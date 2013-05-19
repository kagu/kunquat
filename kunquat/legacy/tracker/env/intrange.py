# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2012
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from PyQt4 import QtCore, QtGui


class IntRange(QtGui.QWidget):

    rangeChanged = QtCore.pyqtSignal(int, name='rangeChanged')

    def __init__(self, index, allow_degen=True, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.index = index
        self._allow_degen = allow_degen
        layout = QtGui.QHBoxLayout(self)
        layout.setMargin(0)
        layout.setSpacing(0)
        self._begin = QtGui.QSpinBox()
        self._begin.setMinimum(-2**31)
        self._begin.setMaximum(2**31 - 1)
        self._end = QtGui.QSpinBox()
        self._end.setMinimum(-2**31)
        self._end.setMaximum(2**31 - 1)
        layout.addWidget(self._begin)
        layout.addWidget(self._end)
        self._degen_fix = None

    def init(self):
        QtCore.QObject.connect(self._begin,
                               QtCore.SIGNAL('valueChanged(int)'),
                               self._begin_changed)
        QtCore.QObject.connect(self._end,
                               QtCore.SIGNAL('valueChanged(int)'),
                               self._end_changed)

    @property
    def range(self):
        return [self._begin.value(), self._end.value()]

    @range.setter
    def range(self, r):
        if not r:
            r = [0, 1]
        if not self._allow_degen and r[0] == r[1]:
            r[1] += 1
        self._begin.blockSignals(True)
        self._end.blockSignals(True)
        try:
            self._begin.setValue(r[0])
            self._end.setValue(r[1])
        finally:
            self._begin.blockSignals(False)
            self._end.blockSignals(False)

    def _fix_val(self, value):
        return value - 1 if value > 0 else value + 1

    def _unfix_val(self, value):
        return value + 1 if value >= 0 else value - 1

    def _begin_changed(self, value):
        self._changed(value, self._begin, self._end)

    def _end_changed(self, value):
        self._changed(value, self._end, self._begin)

    def _changed(self, value, changed, other):
        if not self._allow_degen:
            if changed.value() == other.value():
                fval = other.value()
                if self._degen_fix == other:
                    fval = self._unfix_val(fval)
                    self._degen_fix = None
                else:
                    fval = self._fix_val(fval)
                    self._degen_fix = other
                other.blockSignals(True)
                other.setValue(fval)
                other.blockSignals(False)
            elif self._degen_fix == other and \
                    self._unfix_val(other.value()) != changed.value():
                other.blockSignals(True)
                other.setValue(self._unfix_val(self._end.value()))
                other.blockSignals(False)
                self._degen_fix = None
            if self._degen_fix == changed:
                self._degen_fix = None
        r = self.range
        QtCore.QObject.emit(self,
                            QtCore.SIGNAL('rangeChanged(int)'),
                            self.index)

