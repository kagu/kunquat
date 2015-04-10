# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2015
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

from procnumslider import ProcNumSlider


class FreeverbProc(QWidget):

    @staticmethod
    def get_name():
        return u'Freeverb'

    def __init__(self):
        QWidget.__init__(self)
        self._au_id = None
        self._proc_id = None
        self._ui_model = None

        self._refl = ReflSlider()
        self._damp = DampSlider()

        sliders = QGridLayout()
        sliders.addWidget(QLabel('Reflectivity'), 0, 0)
        sliders.addWidget(self._refl, 0, 1)
        sliders.addWidget(QLabel('Damp'), 1, 0)
        sliders.addWidget(self._damp, 1, 1)

        v = QVBoxLayout()
        v.addLayout(sliders)
        v.addStretch(1)
        self.setLayout(v)

        self.setSizePolicy(QSizePolicy.MinimumExpanding, QSizePolicy.MinimumExpanding)

    def set_au_id(self, au_id):
        self._refl.set_au_id(au_id)
        self._damp.set_au_id(au_id)

    def set_proc_id(self, proc_id):
        self._refl.set_proc_id(proc_id)
        self._damp.set_proc_id(proc_id)

    def set_ui_model(self, ui_model):
        self._refl.set_ui_model(ui_model)
        self._damp.set_ui_model(ui_model)

    def unregister_updaters(self):
        self._damp.unregister_updaters()
        self._refl.unregister_updaters()


class FreeverbSlider(ProcNumSlider):

    def __init__(self, decimals, min_value, max_value):
        ProcNumSlider.__init__(self, decimals, min_value, max_value, '')

    def _get_fv_params(self):
        module = self._ui_model.get_module()
        au = module.get_audio_unit(self._au_id)
        proc = au.get_processor(self._proc_id)
        freeverb_params = proc.get_type_params()
        return freeverb_params


class ReflSlider(FreeverbSlider):

    def __init__(self):
        FreeverbSlider.__init__(self, 1, 0, 200)

    def _get_update_signal_type(self):
        return '_'.join(('signal_freeverb_refl', self._proc_id))

    def _update_value(self):
        fv_params = self._get_fv_params()
        self.set_number(fv_params.get_reflectivity())

    def _value_changed(self, value):
        fv_params = self._get_fv_params()
        fv_params.set_reflectivity(value)
        self._updater.signal_update(set([self._get_update_signal_type()]))


class DampSlider(FreeverbSlider):

    def __init__(self):
        FreeverbSlider.__init__(self, 1, 0, 100)

    def _get_update_signal_type(self):
        return '_'.join(('signal_freeverb_damp', self._proc_id))

    def _update_value(self):
        fv_params = self._get_fv_params()
        self.set_number(fv_params.get_damp())

    def _value_changed(self, value):
        fv_params = self._get_fv_params()
        fv_params.set_damp(value)
        self._updater.signal_update(set([self._get_update_signal_type()]))


