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

from globalforce import GlobalForce
from forcevariation import ForceVariation
from force_env import ForceEnvelope


class ForceEditor(QWidget):

    def __init__(self):
        QWidget.__init__(self)
        self._ui_model = None
        self._ins_id = None
        self._global_force = GlobalForce()
        self._force_var = ForceVariation()
        self._force_env = ForceEnvelope()

        sliders = QGridLayout()
        sliders.addWidget(QLabel('Global force:'), 0, 0)
        sliders.addWidget(self._global_force, 0, 1)
        sliders.addWidget(QLabel('Force variation:'), 1, 0)
        sliders.addWidget(self._force_var, 1, 1)

        v = QVBoxLayout()
        v.addLayout(sliders)
        v.addWidget(self._force_env)
        self.setLayout(v)

    def set_ins_id(self, ins_id):
        self._ins_id = ins_id
        self._global_force.set_ins_id(ins_id)
        self._force_var.set_ins_id(ins_id)
        self._force_env.set_ins_id(ins_id)

    def set_ui_model(self, ui_model):
        self._global_force.set_ui_model(ui_model)
        self._force_var.set_ui_model(ui_model)
        self._force_env.set_ui_model(ui_model)

    def unregister_updaters(self):
        self._force_env.unregister_updaters()
        self._force_var.unregister_updaters()
        self._global_force.unregister_updaters()


