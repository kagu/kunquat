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

from .iacontrols import IAControls
from .saverwindow import SaverWindow
from .updater import Updater
from .utils import get_abs_window_size


class IAWindow(Updater, SaverWindow):

    def __init__(self):
        super().__init__()
        self._ia_controls = IAControls()

        self.add_to_updaters(self._ia_controls)

        self.setWindowTitle('Interactivity controls')

        v = QVBoxLayout()
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(0)
        v.addWidget(self._ia_controls)
        self.setLayout(v)

    def closeEvent(self, event):
        event.ignore()
        visibility_mgr = self._ui_model.get_visibility_manager()
        visibility_mgr.hide_interactivity_controls()

    def sizeHint(self):
        return get_abs_window_size(0.3, 0.4)


