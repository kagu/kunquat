# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2016-2019
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from .procparams import ProcParams


class ForceParams(ProcParams):

    @staticmethod
    def get_default_signal_type():
        return 'voice'

    @staticmethod
    def get_port_info():
        return { 'in_00': 'stretch', 'in_01': 'rel.stch', 'out_00': 'force' }

    def __init__(self, proc_id, controller):
        super().__init__(proc_id, controller)

    def get_edit_set_default_configuration(self):
        transaction = self.get_edit_set_global_force(-8)
        transaction.update(self.get_edit_set_release_ramp_enabled(True))
        return transaction

    def get_global_force(self):
        return self._get_value('p_f_global_force.json', 0.0)

    def get_edit_set_global_force(self, value):
        return self._get_edit_set_value('p_f_global_force.json', value)

    def set_global_force(self, value):
        self._set_value('p_f_global_force.json', value)

    def get_force_variation(self):
        return self._get_value('p_f_force_variation.json', 0.0)

    def set_force_variation(self, value):
        self._set_value('p_f_force_variation.json', value)

    def get_envelope(self):
        ret_env = { 'nodes': [ [0, 1], [1, 1] ], 'marks': [0, 1], 'smooth': False }
        stored_env = self._get_value('p_e_env.json', None) or {}
        ret_env.update(stored_env)
        return ret_env

    def set_envelope(self, envelope):
        self._set_value('p_e_env.json', envelope)

    def get_envelope_enabled(self):
        return self._get_value('p_b_env_enabled.json', False)

    def set_envelope_enabled(self, enabled):
        self._set_value('p_b_env_enabled.json', enabled)

    def get_envelope_loop_enabled(self):
        return self._get_value('p_b_env_loop_enabled.json', False)

    def set_envelope_loop_enabled(self, enabled):
        self._set_value('p_b_env_loop_enabled.json', enabled)

    def get_release_envelope(self):
        ret_env = { 'nodes': [ [0, 1], [1, 0] ], 'smooth': False }
        stored_env = self._get_value('p_e_env_rel.json', None) or {}
        ret_env.update(stored_env)
        return ret_env

    def set_release_envelope(self, envelope):
        self._set_value('p_e_env_rel.json', envelope)

    def get_release_envelope_enabled(self):
        return self._get_value('p_b_env_rel_enabled.json', False)

    def set_release_envelope_enabled(self, enabled):
        self._set_value('p_b_env_rel_enabled.json', enabled)

    def get_release_ramp_enabled(self):
        return self._get_value('p_b_release_ramp.json', False)

    def get_edit_set_release_ramp_enabled(self, enabled):
        return self._get_edit_set_value('p_b_release_ramp.json', enabled)

    def set_release_ramp_enabled(self, enabled):
        self._set_value('p_b_release_ramp.json', enabled)


