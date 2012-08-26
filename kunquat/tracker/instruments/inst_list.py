# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2010-2011
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from PyQt4 import QtCore, QtGui

import kunquat.tracker.kqt_limits as lim


class InstList(QtGui.QTableWidget):

    def __init__(self, p, project, instrument_spin, parent=None):
        QtGui.QTableWidget.__init__(self, lim.INSTRUMENTS_MAX, 1, parent)
        self.p = p
        self.setSelectionMode(QtGui.QAbstractItemView.SingleSelection)

        self._project = project
        self._instrument_spin = instrument_spin
        self.setVerticalHeaderLabels([str(num) for num in
                                      xrange(lim.INSTRUMENTS_MAX)])
        self.setCornerButtonEnabled(False)
        self.setTabKeyNavigation(False)
        self.horizontalHeader().setStretchLastSection(True)
        self.horizontalHeader().hide()

        QtCore.QObject.connect(instrument_spin,
                               QtCore.SIGNAL('currentIndexChanged(const QString&)'),
                               self.inst_changed)
        QtCore.QObject.connect(self,
                    QtCore.SIGNAL('currentCellChanged(int, int, int, int)'),
                               self.cell_changed)
        QtCore.QObject.connect(self,
                               QtCore.SIGNAL('cellChanged(int, int)'),
                               self.name_changed)
        self._signal = False

    def inst_changed(self, text):
        if text == '':
            return
        parts = text.split(':')
        num = int(parts[0] )

        index = self.model().index(num, 0)
        select = self.selectionModel()
        select.clear()
        select.select(index, QtGui.QItemSelectionModel.Select)
        select.setCurrentIndex(index, QtGui.QItemSelectionModel.Select)

    def cell_changed(self, cur_row, cur_col, prev_row, prev_col):
        if self._signal:
            return
        self._signal = True
        if cur_row < self._instrument_spin.count():
            self._instrument_spin.setCurrentIndex(cur_row)
        self._signal = False

    def name_changed(self, num, col):
        assert num >= 0
        assert num < lim.INSTRUMENTS_MAX
        if self._signal:
            return
        item = self.item(num, 0)
        key = 'ins_{0:02x}/kqti{1}/m_name.json'.format(num,
                                                       lim.FORMAT_VERSION)
        if item:
            self._project[key] = unicode(item.text())
        else:
            self._project[key] = None

    def update_instruments(self):
        # FIXME: Remove try-catch
        try:
            inst_num = self.p._instruments._inst_num
        except AttributeError:
            inst_num = 0
        while self.rowCount() > 0:
            self.removeRow(0)
        ids = self.p.project._composition.instrument_ids()
        numbers = [int(i.split('_')[1]) for i in ids]
        header_nums = sorted(numbers)
        self.setRowCount(len(header_nums))
        self.setVerticalHeaderLabels([str(num) for num in header_nums])
        for i in header_nums:
            name = self._project['ins_{0:02x}/kqti{1}/m_name.json'.format(i,
                                                        lim.FORMAT_VERSION)]
            label = name or ''
            item = QtGui.QTableWidgetItem(label)
            self.setItem(i, 0, item)
            if i == inst_num:
                self.setItemSelected(item, True)

    def sync(self):
        self._signal = True
        self.update_instruments()
        '''
        for i in xrange(lim.INSTRUMENTS_MAX):
            name = self._project['ins_{0:02x}/kqti{1}/m_name.json'.format(i,
                                                        lim.FORMAT_VERSION)]
            if name:
                item = self.item(i, 0)
                if not item:
                    item = QtGui.QTableWidgetItem(name)
                    self.setItem(i, 0, item)
                else:
                    item.setText(name)
            else:
                item = self.item(i, 0)
                if item:
                    item.setText('')
        '''
        self._signal = False


