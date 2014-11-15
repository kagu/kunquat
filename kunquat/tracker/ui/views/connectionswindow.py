# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2014
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

from connections import Connections


class ConnectionsWindow(QWidget):

    def __init__(self):
        QWidget.__init__(self)
        self._ui_model = None
        self._connections = Connections()

        self.setWindowTitle('Connections')

        v = QVBoxLayout()
        v.addWidget(self._connections)
        self.setLayout(v)

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model
        self._connections.set_ui_model(ui_model)

    def unregister_updaters(self):
        self._connections.unregister_updaters()

    def closeEvent(self, event):
        event.ignore()
        visibility_manager = self._ui_model.get_visibility_manager()
        visibility_manager.hide_connections()

    def sizeHint(self):
        return QSize(800, 600)


