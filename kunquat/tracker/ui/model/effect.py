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

from kunquat.kunquat.kunquat import get_default_value


class Effect():

    def __init__(self, effect_id):
        assert(effect_id)
        self._effect_id = effect_id
        self._store = None
        self._controller = None
        self._effect_number = None
        self._existence = None

    def set_controller(self, controller):
        self._store = controller.get_store()
        self._controller = controller

    def get_id(self):
        return self._effect_id

    def _get_key(self, subkey):
        return '{}/{}'.format(self._effect_id, subkey)

    def get_existence(self):
        key = self._get_key('p_manifest.json')
        manifest = self._store[key]
        return (type(manifest) == dict)


