# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2016
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


class GeneralModEditor(QWidget):

    def __init__(self):
        QWidget.__init__(self)

        self._title = Title()
        self._authors = Authors()

        self._mixing_volume = MixingVolume()
        self._random_seed = RandomSeed()

        ml = QGridLayout()
        ml.setMargin(0)
        ml.setHorizontalSpacing(4)
        ml.setVerticalSpacing(2)
        ml.addWidget(QLabel('Title:'), 0, 0)
        ml.addWidget(self._title, 0, 1)
        ml.addWidget(QLabel('Authors:'), 1, 0)
        ml.addWidget(self._authors, 1, 1)

        separator = QFrame()
        separator.setFrameShape(QFrame.HLine)
        separator.setFrameShadow(QFrame.Sunken)
        separator.setSizePolicy(QSizePolicy.MinimumExpanding, QSizePolicy.Maximum)
        separator.setMinimumHeight(2)

        gl = QGridLayout()
        gl.setMargin(0)
        gl.setHorizontalSpacing(4)
        gl.setVerticalSpacing(2)
        gl.setColumnStretch(2, 1)
        gl.addWidget(QLabel('Mixing volume:'), 0, 0)
        gl.addWidget(self._mixing_volume, 0, 1)
        gl.addWidget(QWidget(), 0, 2)
        gl.addWidget(QLabel('Initial random seed:'), 1, 0)
        gl.addWidget(self._random_seed, 1, 1)

        v = QVBoxLayout()
        v.setMargin(0)
        v.setSpacing(4)
        v.addLayout(ml)
        v.addWidget(separator)
        v.addLayout(gl)
        v.addStretch(1)
        self.setLayout(v)

    def set_ui_model(self, ui_model):
        self._title.set_ui_model(ui_model)
        self._authors.set_ui_model(ui_model)
        self._mixing_volume.set_ui_model(ui_model)
        self._random_seed.set_ui_model(ui_model)

    def unregister_updaters(self):
        self._random_seed.unregister_updaters()
        self._mixing_volume.unregister_updaters()
        self._authors.unregister_updaters()
        self._title.unregister_updaters()


class Title(QLineEdit):

    def __init__(self):
        QLineEdit.__init__(self)
        self._ui_model = None
        self._updater = None

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model
        self._updater = ui_model.get_updater()
        self._updater.register_updater(self._perform_updates)

        QObject.connect(self, SIGNAL('textEdited(const QString&)'), self._change_title)

        self._update_title()

    def unregister_updaters(self):
        self._updater.unregister_updater(self._perform_updates)

    def _perform_updates(self, signals):
        if 'signal_title' in signals:
            self._update_title()

    def _update_title(self):
        module = self._ui_model.get_module()
        title = module.get_title() or ''

        if self.text() != title:
            old_block = self.blockSignals(True)
            self.setText(title)
            self.blockSignals(old_block)

    def _change_title(self, text_qstring):
        title = str(text_qstring)
        module = self._ui_model.get_module()
        module.set_title(title)
        self._updater.signal_update(set(['signal_title']))


class AuthorTableModel(QAbstractTableModel):

    def __init__(self):
        QAbstractTableModel.__init__(self)
        self._ui_model = None
        self._updater = None

        self._items = ['']

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model
        self._updater = ui_model.get_updater()
        self._make_items()

    def unregister_updaters(self):
        pass

    def _make_items(self):
        module = self._ui_model.get_module()
        count = module.get_author_count()

        self._items = []
        for i in range(count):
            self._items.append(module.get_author(i))
        self._items.append('')

    def get_index(self, author_index):
        return self.createIndex(0, author_index, self._items[author_index])

    # Qt interface

    def columnCount(self, parent):
        if parent.isValid():
            return 0
        return len(self._items)

    def rowCount(self, parent):
        if parent.isValid():
            return 0
        return 1

    def data(self, index, role):
        if role in (Qt.DisplayRole, Qt.EditRole):
            row = index.row()
            column = index.column()
            if row == 0:
                if 0 <= column < len(self._items):
                    return QVariant(self._items[column])

        return QVariant()

    def headerData(self, section, orientation, role):
        return QVariant()

    def flags(self, index):
        default_flags = QAbstractTableModel.flags(self, index)
        if not index.isValid():
            return default_flags
        if index.row() != 0 or not 0 <= index.column() < len(self._items):
            return default_flags

        return default_flags | Qt.ItemIsEditable

    def setData(self, index, value, role):
        if role == Qt.EditRole:
            row = index.row()
            column = index.column()
            if row == 0 and 0 <= column < len(self._items):
                new_name = str(value.toString())
                module = self._ui_model.get_module()
                module.set_author(column, new_name)
                self._updater.signal_update(set(['signal_authors']))
                return True

        return False


class Authors(QTableView):

    def __init__(self):
        QTableView.__init__(self)
        self._ui_model = None
        self._updater = None

        self._model = None

        self.horizontalHeader().hide()
        self.verticalHeader().hide()

        self.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

        self.setEditTriggers(
                QAbstractItemView.SelectedClicked |
                QAbstractItemView.EditKeyPressed |
                QAbstractItemView.AnyKeyPressed)

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model
        self._updater = ui_model.get_updater()
        self._updater.register_updater(self._perform_updates)

        self._update_model()

    def unregister_updaters(self):
        self._updater.unregister_updater(self._perform_updates)

    def _perform_updates(self, signals):
        if 'signal_authors' in signals:
            self._update_model()

    def _update_model(self):
        selected = None
        if self._model:
            selection_model = self.selectionModel()
            current_index = selection_model.currentIndex()
            if current_index.isValid():
                selected = current_index.column()

        self._model = AuthorTableModel()
        self._model.set_ui_model(self._ui_model)
        self.setModel(self._model)

        if selected != None:
            selection_model = self.selectionModel()
            selection_model.setCurrentIndex(
                    self._model.get_index(selected), QItemSelectionModel.SelectCurrent)

        # Remove excess height
        row_height = self.rowHeight(0)
        opt = QStyleOption()
        opt.initFrom(self)
        size = self.style().sizeFromContents(
                QStyle.CT_LineEdit, opt, QSize(1, row_height), self).expandedTo(
                        QApplication.globalStrut())
        self.setFixedHeight(size.height())

        self.resizeColumnsToContents()

    def keyPressEvent(self, event):
        if self.state() != QAbstractItemView.EditingState:
            index = self.currentIndex()
            if event.key() == Qt.Key_Return:
                if index.isValid() and self.edit(index, self.EditKeyPressed, event):
                    event.accept()
                    return
        return QTableView.keyPressEvent(self, event)


class MixingVolume(QDoubleSpinBox):

    def __init__(self):
        QDoubleSpinBox.__init__(self)
        self._ui_model = None
        self._updater = None

        self.setDecimals(2)
        self.setRange(-384, 18)

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model
        self._updater = ui_model.get_updater()
        self._updater.register_updater(self._perform_updates)

        QObject.connect(self, SIGNAL('valueChanged(double)'), self._change_mixing_volume)

        self._update_mixing_volume()

    def unregister_updaters(self):
        self._updater.unregister_updater(self._perform_updates)

    def _perform_updates(self, signals):
        if 'signal_mixing_volume' in signals:
            self._update_mixing_volume()

    def _update_mixing_volume(self):
        module = self._ui_model.get_module()
        mix_vol = module.get_mixing_volume()
        if self.value() != mix_vol:
            old_block = self.blockSignals(True)
            self.setValue(mix_vol)
            self.blockSignals(old_block)

    def _change_mixing_volume(self, value):
        module = self._ui_model.get_module()
        module.set_mixing_volume(value)
        self._updater.signal_update(set(['signal_mixing_volume']))


class RandomSeed(QWidget):

    def __init__(self):
        QWidget.__init__(self)
        self._ui_model = None
        self._updater = None

        self._seed = UInt63SpinBox()
        self._auto_update = QCheckBox('Update automatically')

        h = QHBoxLayout()
        h.setMargin(0)
        h.setSpacing(4)
        h.addWidget(self._seed)
        h.addWidget(self._auto_update)
        self.setLayout(h)

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model
        self._updater = ui_model.get_updater()
        self._updater.register_updater(self._perform_updates)

        QObject.connect(self._seed, SIGNAL('valueChanged()'), self._change_random_seed)
        QObject.connect(
                self._auto_update, SIGNAL('stateChanged(int)'), self._change_auto_update)

        self._update_all()

    def unregister_updaters(self):
        self._updater.unregister_updater(self._perform_updates)

    def _perform_updates(self, signals):
        if 'signal_random_seed' in signals:
            self._update_all()

    def _update_all(self):
        module = self._ui_model.get_module()
        seed = module.get_random_seed()
        auto_update_enabled = module.get_random_seed_auto_update()

        self._seed.set_value(seed)
        self._seed.setEnabled(not auto_update_enabled)

        old_block = self._auto_update.blockSignals(True)
        self._auto_update.setChecked(auto_update_enabled)
        self._auto_update.blockSignals(old_block)

    def _change_random_seed(self):
        module = self._ui_model.get_module()
        module.set_random_seed(self._seed.get_value())
        self._updater.signal_update(set(['signal_random_seed']))

    def _change_auto_update(self, state):
        enabled = (state == Qt.Checked)
        module = self._ui_model.get_module()
        module.set_random_seed_auto_update(enabled)
        self._updater.signal_update(set(['signal_random_seed']))


class UInt63SpinBox(QAbstractSpinBox):

    _LIMIT = 2**63

    valueChanged = pyqtSignal(name='valueChanged')

    def __init__(self):
        QAbstractSpinBox.__init__(self)

        self._value = 0
        self._minimum_size = None

        line_edit = self.lineEdit()
        line_edit.setText(str(self._value))
        QObject.connect(
                line_edit, SIGNAL('textChanged(const QString&)'), self._change_value)

    def set_value(self, value):
        old_block = self.blockSignals(True)
        self._value = value
        self.lineEdit().setText(str(self._value))
        self.blockSignals(old_block)
        self.update()

    def get_value(self):
        return self._value

    def _change_value(self, in_qstring):
        try:
            value = int(str(in_qstring))
        except ValueError:
            return

        self.set_value(value)
        QObject.emit(self, SIGNAL('valueChanged()'))

    def stepEnabled(self):
        if self.wrapping():
            return QAbstractSpinBox.StepUpEnabled | QAbstractSpinBox.StepDownEnabled

        flags = QAbstractSpinBox.StepNone
        if self._value > 0:
            flags |= QAbstractSpinBox.StepDownEnabled
        if self._value < self._LIMIT:
            flags |= QAbstractSpinBox.StepUpEnabled

        return flags

    def stepBy(self, steps):
        self._value += steps

        if self.wrapping():
            while self._value < 0:
                self._value += self._LIMIT
            while self._value >= self._LIMIT:
                self._value -= self._LIMIT
        else:
            self._value = min(max(0, self._value), self._LIMIT - 1)

        line_edit = self.lineEdit()
        old_block = line_edit.blockSignals(True)
        line_edit.setText(str(self._value))
        line_edit.blockSignals(old_block)

        QObject.emit(self, SIGNAL('valueChanged()'))
        self.update()

    def text(self):
        return str(self._value)

    def fixup(self, in_qstring):
        if not in_qstring:
            in_qstring.append('0')
        return QAbstractSpinBox.fixup(self, in_qstring)

    def validate(self, in_qstring, pos):
        in_str = str(in_qstring)
        if not in_str:
            return (QValidator.Intermediate, pos)

        try:
            value = int(in_str)
        except ValueError:
            return (QValidator.Invalid, pos)

        if value >= self._LIMIT:
            return (QValidator.Invalid, pos)

        return (QValidator.Acceptable, pos)

    def minimumSizeHint(self):
        if not self._minimum_size:
            fm = self.fontMetrics()
            width = fm.width(str(self._LIMIT - 1)) + 2
            height = self.lineEdit().minimumSizeHint().height()

            opt = QStyleOptionSpinBox()
            self.initStyleOption(opt)
            self._minimum_size = self.style().sizeFromContents(
                    QStyle.CT_SpinBox, opt, QSize(width, height), self).expandedTo(
                            QApplication.globalStrut())

        return self._minimum_size


