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


class InstrumentEditButton(QToolButton):

    def __init__(self):
        QToolButton.__init__(self)
        self._ui_model = None

        self.setText('Edit')

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model
        QObject.connect(self, SIGNAL('clicked()'), self._clicked)

    def unregister_updaters(self):
        pass

    def _clicked(self):
        module = self._ui_model.get_module()
        ui_manager = self._ui_model.get_ui_manager()
        control_id = ui_manager.get_selected_control_id()
        control = module.get_control(control_id)
        instrument = control.get_instrument()

        visibility_manager = self._ui_model.get_visibility_manager()
        visibility_manager.show_instrument(instrument.get_id())


