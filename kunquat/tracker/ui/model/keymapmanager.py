# -*- coding: utf-8 -*-

#
# Authors: Toni Ruottu, Finland 2014
#          Tomi Jylhä-Ollila, Finland 2016
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#


class HitKeymapID:
    pass


_hit_keymap = {
    'is_hit_keymap': True,
    'name': 'Hits',
    'keymap': [
        [0, 7, 1, 8, 2, 9, 3, 10, 4, 11, 5, 12, 6, 13,
            14, 23, 15, 24, 16, 25, 17, 26, 18, 27, 19, 28, 20, 29, 21, 30, 22, 31],
        [32, 39, 33, 40, 34, 41, 35, 42, 36, 43, 37, 44, 38, 45,
            46, 55, 47, 56, 48, 57, 49, 58, 50, 59, 51, 60, 52, 61, 53, 62, 54, 63],
        [64, 71, 65, 72, 66, 73, 67, 74, 68, 75, 69, 76, 70, 77,
            78, 87, 79, 88, 80, 89, 81, 90, 82, 91, 83, 92, 84, 93, 85, 94, 86, 95],
        [96, 103, 97, 104, 98, 105, 99, 106, 100, 107, 101, 108, 102, 109,
            110, 119, 111, 120, 112, 121, 113, 122, 114, 123, 115, 124, 116, 125,
            117, 126, 118, 127],
    ],
}


class KeymapManager():

    def __init__(self):
        self._session = None
        self._updater = None
        self._keymaps = None
        self._ui_model = None

    def set_controller(self, controller):
        self._session = controller.get_session()
        self._updater = controller.get_updater()
        self._share = controller.get_share()

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model

    def get_keymap_ids(self):
        keymaps = self._share.get_keymaps()
        keymap_ids = keymaps.keys()
        keymap_ids.append(HitKeymapID)
        return keymap_ids

    def get_selected_keymap_id(self):
        keymap_ids = self.get_keymap_ids()
        selected_id = self._session.get_selected_keymap_id()
        if len(keymap_ids) < 2:
            return HitKeymapID
        if not selected_id in keymap_ids:
            some_id = sorted(keymap_ids)[1]
            return some_id
        return selected_id

    def is_hit_keymap_selected(self):
        return (self.get_selected_keymap_id() == HitKeymapID)

    def set_selected_keymap_id(self, keymap_id):
        self._session.set_selected_keymap_id(keymap_id)
        self._updater.signal_update()

    def get_keymap(self, keymap_id):
        if keymap_id == HitKeymapID:
            return _hit_keymap

        keymaps = self._share.get_keymaps()
        keymap = keymaps[keymap_id]
        return keymap

    def get_selected_keymap(self):
        keymap_id = self.get_selected_keymap_id()
        keymap = self.get_keymap(keymap_id)
        return keymap


