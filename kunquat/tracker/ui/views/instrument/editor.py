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

from force_editor import ForceEditor
from infoeditor import InfoEditor


class Editor(QWidget):

    def __init__(self):
        QWidget.__init__(self)
        self._ui_model = None
        self._ins_id = None
        self._tabs = QTabWidget()

        self._force_editor = ForceEditor()
        self._info_editor = InfoEditor()
        self._tabs.addTab(self._force_editor, 'Force')
        self._tabs.addTab(self._info_editor, 'Info')

        v = QVBoxLayout()
        v.setMargin(0)
        v.addWidget(self._tabs)
        self.setLayout(v)

    def set_ins_id(self, ins_id):
        self._ins_id = ins_id
        self._force_editor.set_ins_id(ins_id)
        self._info_editor.set_ins_id(ins_id)

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model
        self._force_editor.set_ui_model(ui_model)
        self._info_editor.set_ui_model(ui_model)

    def unregister_updaters(self):
        self._info_editor.unregister_updaters()
        self._force_editor.unregister_updaters()

