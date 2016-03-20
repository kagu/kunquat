# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2014-2016
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from copy import deepcopy


class Notation():

    def __init__(self, notation_id):
        self._controller = None
        self._store = None
        self._share = None

        is_shared, dict_id = notation_id
        self._is_shared = is_shared
        self._id = dict_id

    def set_controller(self, controller):
        self._controller = controller
        self._store = controller.get_store()
        self._share = controller.get_share()

    def get_existence(self):
        if self._is_shared:
            return self._id in self._share.get_notations()
        else:
            return self._id in self._store.get('i_notations.json', [])

    def _get_raw_data(self):
        if self._is_shared:
            return self._share.get_notations()[self._id]
        else:
            return self._store['i_notations.json'][self._id]

    def _set_raw_data(self, raw_data):
        assert not self._is_shared
        notations = self._store['i_notations.json']
        notations[self._id] = raw_data
        self._store['i_notations.json'] = notations

    def get_name(self):
        return self._get_raw_data()['name']

    def set_name(self, name):
        data = deepcopy(self._get_raw_data())
        data['name'] = name
        self._set_raw_data(data)

    def get_octave_count(self):
        return len(self._get_raw_data()['octave_names'])

    def add_octave(self):
        data = deepcopy(self._get_raw_data())
        data['octave_names'].append(u'{}'.format(self.get_octave_count()))
        self._set_raw_data(data)

    def remove_octave(self, octave_id):
        assert self.get_octave_count() > 1
        data = deepcopy(self._get_raw_data())
        del data['octave_names'][octave_id]
        self._set_raw_data(data)

    def get_octave_name(self, octave_id):
        return self._get_raw_data()['octave_names'][octave_id]

    def set_octave_name(self, octave_id, name):
        data = deepcopy(self._get_raw_data())
        data['octave_names'][octave_id] = name
        self._set_raw_data(data)

    def get_base_octave_id(self):
        return self._get_raw_data()['base_octave']

    def set_base_octave_id(self, octave_id):
        assert 0 <= octave_id < self.get_octave_count()
        data = deepcopy(self._get_raw_data())
        data['base_octave'] = octave_id
        self._set_raw_data(data)

    def get_notes(self):
        return (note for note in self._get_raw_data()['note_names'])

    def get_note(self, index):
        return self._get_raw_data()['note_names'][index]

    def set_note_cents(self, index, cents):
        data = deepcopy(self._get_raw_data())
        data['note_names'][index][0] = cents
        self._set_raw_data(data)

    def set_note_name(self, index, name):
        data = deepcopy(self._get_raw_data())
        data['note_names'][index][1] = name
        self._set_raw_data(data)

    def add_note(self):
        data = self._get_raw_data()
        data['note_names'].append((0, 'n{}'.format(len(data['note_names']))))
        self._set_raw_data(data)

    def remove_note(self, index):
        data = self._get_raw_data()
        del data['note_names'][index]
        self._set_raw_data(data)

    def get_note_name_and_offset(self, cents):
        nearest = self._get_nearest_note(cents)
        if not nearest:
            return None

        c, name = nearest
        return (name, cents - c)

    def get_full_name(self, cents):
        base, offset = self.get_note_name_and_offset(cents)
        offset_rnd = int(round(offset))
        if offset_rnd != 0:
            name = '{}{:+d}'.format(base, offset_rnd)
        else:
            name = base
        return name

    def get_keymap(self):
        # TODO: clean up the interface mess
        data = self._get_raw_data()
        keymap = {
            'name':        data['name'],
            'base_octave': data['base_octave'],
            'keymap':      deepcopy(data['keymap']),
        }
        return keymap

    def _get_nearest_note(self, cents):
        nearest = None
        nearest_dist = float('inf')

        notes = self._get_raw_data()['note_names']
        for note in notes:
            c, name = note
            dist = abs(c - cents)
            if dist < nearest_dist or (dist == nearest_dist and name < nearest[1]):
                nearest = note
                nearest_dist = dist

        return nearest


