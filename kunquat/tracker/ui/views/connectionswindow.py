# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2014-2018
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from kunquat.tracker.ui.qt import *

from .connectionseditor import ConnectionsEditor
from .keyboardmapper import KeyboardMapper
from .saverwindow import SaverWindow
from .updater import Updater
from .utils import get_abs_window_size


class ConnectionsWindow(Updater, SaverWindow):

    def __init__(self):
        super().__init__()
        self._ui_model = None
        self._conns_editor = ConnectionsEditor()

        self._keyboard_mapper = KeyboardMapper()

        self.add_to_updaters(self._conns_editor, self._keyboard_mapper)

        self.setWindowTitle('Connections')

        v = QVBoxLayout()
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(0)
        v.addWidget(self._conns_editor)
        self.setLayout(v)

    def closeEvent(self, event):
        event.ignore()
        visibility_mgr = self._ui_model.get_visibility_manager()
        visibility_mgr.hide_connections()

    def sizeHint(self):
        return get_abs_window_size(0.6, 0.7)

    def keyPressEvent(self, event):
        if not self._keyboard_mapper.process_typewriter_button_event(event):
            super().keyPressEvent(event)

    def keyReleaseEvent(self, event):
        if not self._keyboard_mapper.process_typewriter_button_event(event):
            event.ignore()


