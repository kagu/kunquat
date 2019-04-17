# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2014-2019
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from itertools import chain

from kunquat.tracker.ui.qt import *

from kunquat.tracker.ui.views.kqtcombobox import KqtComboBox
from .aukeyboardmapper import AudioUnitKeyboardMapper
from .aunumslider import AuNumSlider
from .components import Components
from .expressions import Expressions
from .hits import Hits
from .ports import Ports
from .infoeditor import InfoEditor
from .testbutton import TestButton
from .audiounitupdater import AudioUnitUpdater


class Editor(QWidget, AudioUnitUpdater):

    def __init__(self):
        super().__init__()
        self._ui_model = None
        self._au_id = None
        self._control_mgr = None

        self._test_panel = TestPanel()
        self._tabs = QTabWidget()

        self._components = Components()
        self._hits = Hits()
        self._expressions = Expressions()
        self._ports = Ports()
        self._info_editor = InfoEditor()

        self._keyboard_mapper = AudioUnitKeyboardMapper()

    def _on_setup(self):
        self.add_to_updaters(
                self._test_panel,
                self._components,
                self._hits,
                self._expressions,
                self._ports,
                self._info_editor,
                self._keyboard_mapper)

        self._control_mgr = self._ui_model.get_control_manager()

        module = self._ui_model.get_module()
        au = module.get_audio_unit(self._au_id)

        self._tabs.addTab(self._components, 'Components')
        if au.is_instrument():
            self._tabs.addTab(self._hits, 'Hits')
            self._tabs.addTab(self._expressions, 'Expressions')
        self._tabs.addTab(self._ports, 'Ports')
        self._tabs.addTab(self._info_editor, 'Info')

        v = QVBoxLayout()
        v.setContentsMargins(4, 4, 4, 4)
        v.setSpacing(4)
        if au.is_instrument():
            v.addWidget(self._test_panel)
        v.addWidget(self._tabs)
        self.setLayout(v)

        self.register_action('signal_style_changed', self._update_style)

        self._update_style()

    def _update_style(self):
        style_mgr = self._ui_model.get_style_manager()
        margin = style_mgr.get_scaled_size_param('medium_padding')
        spacing = style_mgr.get_scaled_size_param('medium_padding')
        self.layout().setContentsMargins(margin, margin, margin, margin)
        self.layout().setSpacing(spacing)

    def keyPressEvent(self, event):
        if not self._keyboard_mapper.process_typewriter_button_event(event):
            event.ignore()

    def keyReleaseEvent(self, event):
        if not self._keyboard_mapper.process_typewriter_button_event(event):
            event.ignore()


class TestPanel(QWidget, AudioUnitUpdater):

    def __init__(self):
        super().__init__()
        self._test_button = TestButton()
        self._test_force = TestForce()
        self._expressions = [TestExpression(i) for i in range(2)]

        self.add_to_updaters(self._test_button, self._test_force, *self._expressions)

        h = QHBoxLayout()
        h.setContentsMargins(0, 0, 0, 0)
        h.setSpacing(4)
        h.addWidget(self._test_button, 1)
        h.addWidget(self._test_force, 1)
        h.addWidget(QLabel('Channel expression:'))
        h.addWidget(self._expressions[0])
        h.addWidget(QLabel('Note expression:'))
        h.addWidget(self._expressions[1])
        self.setLayout(h)

    def _on_setup(self):
        self.register_action('signal_style_changed', self._update_style)
        self._update_style()

    def _update_style(self):
        style_mgr = self._ui_model.get_style_manager()
        self.layout().setSpacing(style_mgr.get_scaled_size_param('medium_padding'))


class TestForce(AuNumSlider):

    def __init__(self):
        super().__init__(1, -30, 12, 'Force:')

    def _get_update_signal_type(self):
        return 'signal_au_test_force_{}'.format(self._au_id)

    def _get_audio_unit(self):
        module = self._ui_model.get_module()
        return module.get_audio_unit(self._au_id)

    def _update_value(self):
        au = self._get_audio_unit()
        self.set_number(au.get_test_force())

    def _value_changed(self, new_value):
        au = self._get_audio_unit()
        au.set_test_force(new_value)
        self._updater.signal_update(self._get_update_signal_type())


class TestExpression(KqtComboBox, AudioUnitUpdater):

    def __init__(self, index):
        super().__init__()
        self._index = index

        self.setSizePolicy(QSizePolicy.Maximum, QSizePolicy.Preferred)
        self.setSizeAdjustPolicy(QComboBox.AdjustToContents)

    def _on_setup(self):
        self.register_action(
                'signal_expr_list_{}'.format(self._au_id), self._update_expression_list)

        self.currentIndexChanged.connect(self._change_expression)

        if self._index == 1:
            # Apply instrument default as the initial note expression
            module = self._ui_model.get_module()
            au = module.get_audio_unit(self._au_id)
            if (au.get_test_expression(self._index) == None and
                    au.get_test_expression(1 - self._index) == None):
                note_expr = au.get_default_note_expression()
                if note_expr:
                    au.set_test_expression(self._index, note_expr)
                else:
                    expressions = au.get_expression_names()
                    if expressions:
                        # Pick some expression by default
                        # to avoid unpleasant surprise when testing the instrument
                        au.set_test_expression(self._index, min(expressions))

        self._update_expression_list()

    def _update_expression_list(self):
        module = self._ui_model.get_module()
        au = module.get_audio_unit(self._au_id)

        expr_names = sorted(au.get_expression_names())

        old_block = self.blockSignals(True)
        self.setEnabled(len(expr_names) > 0)
        self.set_items(chain(['(none)'], (name for name in expr_names)))
        self.setCurrentIndex(0)
        for i, expr_name in enumerate(expr_names):
            if au.get_test_expression(self._index) == expr_name:
                self.setCurrentIndex(i + 1) # + 1 compensates for the (none) entry
        self.blockSignals(old_block)

    def _change_expression(self, item_index):
        module = self._ui_model.get_module()
        au = module.get_audio_unit(self._au_id)
        if item_index == 0:
            au.set_test_expression(self._index, '')
        else:
            expr_name = str(self.itemText(item_index))
            au.set_test_expression(self._index, expr_name)


