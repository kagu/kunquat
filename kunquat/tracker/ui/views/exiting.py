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

from .confirmdialog import ConfirmDialog
from .saving import get_module_save_path


class ExitHelper():

    def __init__(self):
        super().__init__()
        self._ui_model = None
        self._quit_after_saving = False

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model

    def notify_save_module_finished(self):
        if self._quit_after_saving:
            visibility_mgr = self._ui_model.get_visibility_manager()
            visibility_mgr.hide_main_after_saving()

    def try_exit(self):
        module = self._ui_model.get_module()
        if module.is_modified():
            dialog = ExitUnsavedConfirmDialog(
                    self._ui_model,
                    self._perform_save_and_close,
                    self._perform_discard_and_close)
            dialog.exec_()
        else:
            visibility_mgr = self._ui_model.get_visibility_manager()
            visibility_mgr.hide_main()

    def _perform_save_and_close(self):
        module = self._ui_model.get_module()

        if not module.get_path():
            module_path = get_module_save_path()
            if not module_path:
                return
            module.set_path(module_path)

        self._quit_after_saving = True

        module.start_save()

    def _perform_discard_and_close(self):
        visibility_mgr = self._ui_model.get_visibility_manager()
        visibility_mgr.hide_main()


class ExitUnsavedConfirmDialog(ConfirmDialog):

    def __init__(self, ui_model, action_save, action_discard):
        super().__init__(ui_model)

        self._action_save = action_save
        self._action_discard = action_discard

        self.setWindowTitle('Unsaved changes')

        msg = 'There are unsaved changes in the project.'
        self._set_message(msg)

        self._discard_button = QPushButton('Discard changes and exit')
        self._cancel_button = QPushButton('Keep the project open')
        self._save_button = QPushButton('Save changes and exit')

        b = self._get_button_layout()
        b.addWidget(self._discard_button)
        b.addWidget(self._cancel_button)
        b.addWidget(self._save_button)

        self._cancel_button.setFocus(Qt.PopupFocusReason)

        self._save_button.clicked.connect(self._perform_save)
        self._discard_button.clicked.connect(self._perform_discard)
        self._cancel_button.clicked.connect(self.close)

    def _perform_save(self):
        self._action_save()
        self.close()

    def _perform_discard(self):
        self._action_discard()
        self.close()


