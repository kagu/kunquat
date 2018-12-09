# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2017-2018
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from kunquat.tracker.ui.qt import *

from .updater import Updater


class HitMapToggle(QCheckBox, Updater):

    def __init__(self):
        super().__init__('Use hit keymap')
        self._ui_model = None
        self._updater = None

        self.setToolTip('Use hit keymap (Ctrl + H)')
        self.setFocusPolicy(Qt.NoFocus)

    def _on_setup(self):
        self.register_action('signal_select_keymap', self._update_checked)

        self.stateChanged.connect(self._set_hit_map_active)

        self._update_checked()

    def _update_checked(self):
        keymap_mgr = self._ui_model.get_keymap_manager()
        is_active = keymap_mgr.is_hit_keymap_active()
        is_checked = (self.checkState() == Qt.Checked)
        if is_checked != is_active:
            old_block = self.blockSignals(True)
            self.setCheckState(Qt.Checked if is_active else Qt.Unchecked)
            self.blockSignals(old_block)

    def _set_hit_map_active(self, state):
        active = (state == Qt.Checked)
        keymap_mgr = self._ui_model.get_keymap_manager()
        keymap_mgr.set_hit_keymap_active(active)
        self._updater.signal_update('signal_select_keymap')


