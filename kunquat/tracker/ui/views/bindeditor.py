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

from kunquat.tracker.ui.qt import *

import kunquat.kunquat.events as events
from kunquat.kunquat.limits import *
from .editorlist import EditorList
from .headerline import HeaderLine
from .iconbutton import IconButton
from .kqtcombobox import KqtComboBox
from .updater import Updater


class BindEditor(QWidget, Updater):

    def __init__(self):
        super().__init__()
        self._bind_list = BindList()
        self._source_event = SourceEventSelector()
        self._constraints = Constraints()
        self._targets = Targets()

        v = QVBoxLayout()
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(2)
        v.addWidget(HeaderLine('Event bindings'))
        v.addWidget(self._bind_list)
        v.addWidget(self._source_event)
        v.addWidget(self._constraints)
        v.addWidget(self._targets)
        self.setLayout(v)

    def _on_setup(self):
        self.add_to_updaters(
                self._bind_list, self._source_event, self._constraints, self._targets)
        self.register_action('signal_bind', self._update_editor_enabled)

        self._update_editor_enabled()

    def _update_editor_enabled(self):
        bindings = self._ui_model.get_module().get_bindings()
        enable_editor = bindings.has_selected_binding()
        self._source_event.setEnabled(enable_editor)
        self._constraints.setEnabled(enable_editor)
        self._targets.setEnabled(enable_editor)


class BindListToolBar(QToolBar, Updater):

    def __init__(self):
        super().__init__()
        self._add_button = QToolButton()
        self._add_button.setText('Add binding')
        self._add_button.setEnabled(True)

        self._remove_button = QToolButton()
        self._remove_button.setText('Remove binding')
        self._remove_button.setEnabled(False)

        self.addWidget(self._add_button)
        self.addWidget(self._remove_button)

    def _on_setup(self):
        self.register_action('signal_bind', self._update_enabled)

        self._add_button.clicked.connect(self._add_binding)
        self._remove_button.clicked.connect(self._remove_binding)

        self._update_enabled()

    def _update_enabled(self):
        bindings = self._ui_model.get_module().get_bindings()
        remove_enabled = ((bindings.get_count() > 0) and
                (bindings.get_selected_binding_index() != None))
        self._remove_button.setEnabled(remove_enabled)

    def _add_binding(self):
        bindings = self._ui_model.get_module().get_bindings()
        bindings.add_binding()
        self._updater.signal_update('signal_bind')

    def _remove_binding(self):
        bindings = self._ui_model.get_module().get_bindings()
        selected_index = bindings.get_selected_binding_index()
        if selected_index != None:
            bindings.remove_binding(selected_index)
            bindings.set_selected_binding_index(None)
            self._updater.signal_update('signal_bind')


class BindListModel(QAbstractListModel, Updater):

    def __init__(self):
        super().__init__()
        self._items = []

    def _on_setup(self):
        self._make_items()

    def get_item(self, index):
        row = index.row()
        if 0 <= row < len(self._items):
            item = self._items[row]
            return item
        return None

    def _make_items(self):
        bindings = self._ui_model.get_module().get_bindings()
        bs = (bindings.get_binding(i) for i in range(bindings.get_count()))
        self._items = [(i, b.get_source_event()) for (i, b) in enumerate(bs)]

    def get_index(self, list_index):
        return self.createIndex(list_index, 0, self._items[list_index])

    # Qt interface

    def rowCount(self, parent):
        return len(self._items)

    def data(self, index, role):
        if role == Qt.DisplayRole:
            row = index.row()
            if 0 <= row < len(self._items):
                _, event_name = self._items[row]
                return event_name

        return None

    def headerData(self, section, orientation, role):
        return None


class BindListView(QListView, Updater):

    def __init__(self):
        super().__init__()
        self.setSelectionMode(QAbstractItemView.SingleSelection)

    def _select_entry(self, cur_index, prev_index):
        item = self.model().get_item(cur_index)
        if item:
            index, _ = item
            bindings = self._ui_model.get_module().get_bindings()
            bindings.set_selected_binding_index(index)
            self._updater.signal_update('signal_bind')

    def setModel(self, model):
        super().setModel(model)

        selection_model = self.selectionModel()

        bindings = self._ui_model.get_module().get_bindings()
        selected_index = bindings.get_selected_binding_index()
        if selected_index != None:
            selection_model.select(
                    model.get_index(selected_index), QItemSelectionModel.Select)

        selection_model.currentChanged.connect(self._select_entry)


class BindList(QWidget, Updater):

    def __init__(self):
        super().__init__()
        self._toolbar = BindListToolBar()

        self._list_model = None
        self._list_view = BindListView()

        v = QVBoxLayout()
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(0)
        v.addWidget(self._toolbar)
        v.addWidget(self._list_view)
        self.setLayout(v)

    def _on_setup(self):
        self.add_to_updaters(self._toolbar, self._list_view)
        self.register_action('signal_bind', self._update_model)

        self._update_model()

    def _update_model(self):
        if self._list_model:
            self.remove_from_updaters(self._list_model)
        self._list_model = BindListModel()
        self.add_to_updaters(self._list_model)
        self._list_view.setModel(self._list_model)


class EventBox(KqtComboBox):

    def __init__(self, excluded=set()):
        super().__init__()
        self.setSizePolicy(QSizePolicy.MinimumExpanding, QSizePolicy.Preferred)
        self.update_names(excluded)

    def update_names(self, excluded=set()):
        selected = None
        selected_index = self.currentIndex()
        if selected_index != -1:
            selected = str(self.itemText(selected_index))

        all_events = events.all_events_by_name
        event_names = sorted(list(
            event['name'] for event in all_events.values()
            if event['name'] not in excluded),
            key=lambda x: x.lstrip('/=.->+<') or x)

        self.set_items(name for name in event_names)

        if selected:
            self.try_select_event(selected)

    def try_select_event(self, event_name):
        old_block = self.blockSignals(True)
        index = self.findText(event_name)
        if index != -1:
            self.setCurrentIndex(index)
        self.blockSignals(old_block)

    def get_selected_event(self):
        index = self.currentIndex()
        if index == -1:
            return None
        return str(self.itemText(index))


class SourceEventSelector(QWidget, Updater):

    def __init__(self):
        super().__init__()
        self._selector = EventBox()

        h = QHBoxLayout()
        h.setContentsMargins(0, 0, 0, 0)
        h.setSpacing(2)
        h.addWidget(QLabel('Event:'))
        h.addWidget(self._selector)
        self.setLayout(h)

    def _on_setup(self):
        self.register_action('signal_bind', self._update_event)

        self._selector.currentIndexChanged.connect(self._change_event)

        self._update_event()

    def _update_event(self):
        bindings = self._ui_model.get_module().get_bindings()
        if not bindings.has_selected_binding():
            return

        binding = bindings.get_selected_binding()

        excluded = binding.get_excluded_source_events()
        self._selector.update_names(excluded)

        event_name = binding.get_source_event()
        self._selector.try_select_event(event_name)

    def _change_event(self, index):
        new_event = self._selector.get_selected_event()
        bindings = self._ui_model.get_module().get_bindings()
        binding = bindings.get_selected_binding()
        binding.set_source_event(new_event)
        self._updater.signal_update('signal_bind')


class TightLabel(QLabel):

    def __init__(self, text):
        super().__init__(text)
        self.setSizePolicy(QSizePolicy.Maximum, QSizePolicy.Preferred)


class Constraints(QWidget, Updater):

    def __init__(self):
        super().__init__()

        self._cblist = ConstraintList()
        self.add_to_updaters(self._cblist)

        v = QVBoxLayout()
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(2)
        v.addWidget(HeaderLine('Binding constraints'))
        v.addWidget(self._cblist)
        self.setLayout(v)


class ConstraintList(EditorList, Updater):

    def __init__(self):
        super().__init__()

    def _on_setup(self):
        self.register_action('signal_bind', self._update_all)
        self._update_all()

    def _on_teardown(self):
        self.disconnect_widgets()

    def _make_adder_widget(self):
        adder = ConstraintAdder()
        self.add_to_updaters(adder)
        return adder

    def _get_updated_editor_count(self):
        bindings = self._ui_model.get_module().get_bindings()
        if not bindings.has_selected_binding():
            return 0

        constraints = bindings.get_selected_binding().get_constraints()
        return constraints.get_count()

    def _make_editor_widget(self, index):
        editor = ConstraintEditor(index)
        self.add_to_updaters(editor)
        return editor

    def _update_editor(self, index, editor):
        pass

    def _disconnect_widget(self, widget):
        self.remove_from_updaters(widget)

    def _update_all(self):
        self.update_list()


class ConstraintAdder(QPushButton, Updater):

    def __init__(self):
        super().__init__()
        self.setText('Add constraint')

    def _on_setup(self):
        self.clicked.connect(self._add_constraint)

    def _add_constraint(self):
        bindings = self._ui_model.get_module().get_bindings()
        binding = bindings.get_selected_binding()
        binding.get_constraints().add_constraint()
        self._updater.signal_update('signal_bind')


class ConstraintEditor(QWidget, Updater):

    def __init__(self, index):
        super().__init__()
        self._index = index

        self._event = EventBox()

        self._expression = QLineEdit()

        self._remove_button = IconButton()

        self.add_to_updaters(self._remove_button)

        h = QHBoxLayout()
        h.setContentsMargins(0, 0, 0, 0)
        h.setSpacing(2)
        h.addWidget(self._event)
        h.addWidget(TightLabel('Test expression:'))
        h.addWidget(self._expression)
        h.addWidget(self._remove_button)
        self.setLayout(h)

    def _on_setup(self):
        self.register_action('signal_bind', self._update_all)

        style_mgr = self._ui_model.get_style_manager()

        self._remove_button.set_icon('delete_small')
        self._remove_button.set_sizes(style_mgr.get_style_param('list_button_size'),
                style_mgr.get_style_param('list_button_padding'))

        self._event.currentIndexChanged.connect(self._change_event)

        self._expression.editingFinished.connect(self._change_expression)

        self._remove_button.clicked.connect(self._remove)

        self._update_all()

    def _update_all(self):
        bindings = self._ui_model.get_module().get_bindings()
        if not bindings.has_selected_binding():
            return

        binding = bindings.get_selected_binding()
        constraints = binding.get_constraints()
        if self._index >= constraints.get_count():
            return

        constraint = constraints.get_constraint(self._index)

        event_name = constraint.get_event_name()
        self._event.try_select_event(event_name)

        old_block = self._expression.blockSignals(True)
        new_expression = constraint.get_expression()
        if self._expression.text() != new_expression:
            self._expression.setText(new_expression)
        self._expression.blockSignals(old_block)

    def _get_constraint(self):
        bindings = self._ui_model.get_module().get_bindings()
        binding = bindings.get_selected_binding()
        constraint = binding.get_constraints().get_constraint(self._index)
        return constraint

    def _change_event(self, index):
        event_name = self._event.get_selected_event()
        constraint = self._get_constraint()
        constraint.set_event_name(event_name)
        self._updater.signal_update('signal_bind')

    def _change_expression(self):
        expression = str(self._expression.text())
        constraint = self._get_constraint()
        constraint.set_expression(expression)
        self._updater.signal_update('signal_bind')

    def _remove(self):
        bindings = self._ui_model.get_module().get_bindings()
        binding = bindings.get_selected_binding()
        binding.get_constraints().remove_constraint(self._index)
        self._updater.signal_update('signal_bind')


class Targets(QWidget, Updater):

    def __init__(self):
        super().__init__()

        self._target_list = TargetList()
        self.add_to_updaters(self._target_list)

        v = QVBoxLayout()
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(2)
        v.addWidget(HeaderLine('Event targets'))
        v.addWidget(self._target_list)
        self.setLayout(v)


class TargetList(EditorList, Updater):

    def __init__(self):
        super().__init__()

    def _on_setup(self):
        self.register_action('signal_bind', self._update_all)

        self._update_all()

    def _on_teardown(self):
        self.disconnect_widgets()

    def _make_adder_widget(self):
        adder = TargetAdder()
        self.add_to_updaters(adder)
        return adder

    def _get_updated_editor_count(self):
        bindings = self._ui_model.get_module().get_bindings()
        if not bindings.has_selected_binding():
            return 0

        targets = bindings.get_selected_binding().get_targets()
        return targets.get_count()

    def _make_editor_widget(self, index):
        editor = TargetEditor(index)
        self.add_to_updaters(editor)
        return editor

    def _update_editor(self, index, editor):
        pass

    def _disconnect_widget(self, widget):
        self.remove_from_updaters(widget)

    def _update_all(self):
        self.update_list()


class TargetAdder(QPushButton, Updater):

    def __init__(self):
        super().__init__()
        self.setText('Add event target')

    def _on_setup(self):
        self.clicked.connect(self._add_target)

    def _add_target(self):
        bindings = self._ui_model.get_module().get_bindings()
        binding = bindings.get_selected_binding()
        binding.get_targets().add_target()
        self._updater.signal_update('signal_bind')


class TargetEditor(QWidget, Updater):

    def __init__(self, index):
        super().__init__()
        self._index = index

        self._ch_offset = QSpinBox()
        self._ch_offset.setRange(-CHANNELS_MAX + 1, CHANNELS_MAX - 1)

        self._event = EventBox()

        self._expression = QLineEdit()

        self._remove_button = IconButton()

        self.add_to_updaters(self._remove_button)

        h = QHBoxLayout()
        h.setContentsMargins(0, 0, 0, 0)
        h.setSpacing(2)
        h.addWidget(TightLabel('Channel offset:'))
        h.addWidget(self._ch_offset)
        h.addWidget(self._event)
        h.addWidget(TightLabel('Expression:'))
        h.addWidget(self._expression)
        h.addWidget(self._remove_button)
        self.setLayout(h)

    def _on_setup(self):
        self.register_action('signal_bind', self._update_all)

        style_mgr = self._ui_model.get_style_manager()

        self._remove_button.set_icon('delete_small')
        self._remove_button.set_sizes(style_mgr.get_style_param('list_button_size'),
                style_mgr.get_style_param('list_button_padding'))

        self._ch_offset.valueChanged.connect(self._change_ch_offset)
        self._event.currentIndexChanged.connect(self._change_event)
        self._expression.editingFinished.connect(self._change_expression)
        self._remove_button.clicked.connect(self._remove)

        self._update_all()

    def _update_all(self):
        bindings = self._ui_model.get_module().get_bindings()
        if not bindings.has_selected_binding():
            return

        binding = bindings.get_selected_binding()
        targets = binding.get_targets()
        if self._index >= targets.get_count():
            return

        target = targets.get_target(self._index)

        old_block = self._ch_offset.blockSignals(True)
        new_ch_offset = target.get_channel_offset()
        if self._ch_offset.value() != new_ch_offset:
            self._ch_offset.setValue(new_ch_offset)
        self._ch_offset.blockSignals(old_block)

        excluded = target.get_excluded_events()
        self._event.update_names(excluded)

        event_name = target.get_event_name()
        self._event.try_select_event(event_name)

        all_events = events.all_events_by_name

        old_block = self._expression.blockSignals(True)
        if all_events[event_name]['arg_type'] != None:
            new_expression = target.get_expression()
            if self._expression.text() != new_expression:
                self._expression.setText(new_expression)
            self._expression.setEnabled(True)
        else:
            self._expression.setEnabled(False)
            self._expression.setText('')
        self._expression.blockSignals(old_block)

    def _get_target(self):
        bindings = self._ui_model.get_module().get_bindings()
        binding = bindings.get_selected_binding()
        target = binding.get_targets().get_target(self._index)
        return target

    def _change_ch_offset(self, new_offset):
        target = self._get_target()
        target.set_channel_offset(new_offset)
        self._updater.signal_update('signal_bind')

    def _change_event(self, index):
        event_name = self._event.get_selected_event()
        target = self._get_target()

        all_events = events.all_events_by_name
        if all_events[event_name]['arg_type'] != None:
            expression = str(self._expression.text())
        else:
            expression = None

        target.set_event_info(event_name, expression)
        self._updater.signal_update('signal_bind')

    def _change_expression(self):
        expression = str(self._expression.text())
        target = self._get_target()
        target.set_event_info(self._event.get_selected_event(), expression)
        self._updater.signal_update('signal_bind')

    def _remove(self):
        bindings = self._ui_model.get_module().get_bindings()
        binding = bindings.get_selected_binding()
        binding.get_targets().remove_target(self._index)
        self._updater.signal_update('signal_bind')


