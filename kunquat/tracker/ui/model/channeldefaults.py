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

from kunquat.kunquat.kunquat import get_default_value
from control import Control


CHANNELS_MAX = 64 # TODO: Define in Kunquat interface...


class ChannelDefaults():

    def __init__(self, song_id):
        self._song_id = song_id
        self._controller = None
        self._ui_model = None
        self._store = None

    def set_controller(self, controller):
        self._controller = controller
        self._store = controller.get_store()

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model

    def _get_key(self):
        return '{}/p_channel_defaults.json'.format(self._song_id)

    def _get_default_entry(self):
        default_list = get_default_value(self._get_key())
        return default_list[0]

    def _get_entry(self, ch_num):
        key = self._get_key()
        ret_entry = self._get_default_entry()
        stored_list = self._store.get(key, [])
        if ch_num >= len(stored_list):
            return ret_entry
        ret_entry.update(stored_list[ch_num])
        return ret_entry

    def get_default_control_id(self, ch_num):
        entry = self._get_entry(ch_num)
        return 'control_{:02x}'.format(entry['control'])

    def set_default_control_id(self, ch_num, control_id):
        key = self._get_key()
        chd_list = self._store.get(key, [])
        def_entry = self._get_default_entry()
        for _ in xrange(ch_num + 1 - len(chd_list)):
            chd_list.append(def_entry.copy())

        control_id_parts = control_id.split('_')
        control_num = int(control_id_parts[1], 16)

        entry = chd_list[ch_num]
        entry['control'] = control_num

        self._store[key] = chd_list

    def get_edit_remove_controls_to_audio_unit(self, au_id, fallback_control_id):
        transaction = {}

        for ch_num in xrange(CHANNELS_MAX):
            control_id = self.get_default_control_id(ch_num)
            if control_id != fallback_control_id:
                control = Control(control_id)
                control.set_controller(self._controller)
                control.set_ui_model(self._ui_model)
                au = control.get_audio_unit()
                if au_id == au.get_id():
                    self.set_default_control_id(ch_num, fallback_control_id)

        return transaction

