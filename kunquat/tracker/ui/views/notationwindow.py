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

from .notationeditor import NotationEditor
from .saverwindow import SaverWindow
from .updater import Updater
from .utils import get_abs_window_size


class NotationWindow(Updater, SaverWindow):

    def __init__(self):
        super().__init__()
        self._ui_model = None

        self.setWindowTitle('Notations')

        self._editor = NotationEditor()
        self.add_to_updaters(self._editor)

        v = QVBoxLayout()
        v.setContentsMargins(4, 4, 4, 4)
        v.setSpacing(4)
        v.addWidget(self._editor)
        self.setLayout(v)

    def closeEvent(self, event):
        event.ignore()
        visibility_mgr = self._ui_model.get_visibility_manager()
        visibility_mgr.hide_notation_editor()

    def sizeHint(self):
        return get_abs_window_size(0.6, 0.7)


