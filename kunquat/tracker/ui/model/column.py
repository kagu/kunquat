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

from trigger import Trigger
import tstamp


COLUMNS_MAX = 64 # TODO: define in libkunquat interface


class Column():

    def __init__(self, pattern_id, col_num):
        assert pattern_id
        assert 0 <= col_num < COLUMNS_MAX
        self._pattern_id = pattern_id
        self._col_num = col_num
        self._trigger_rows = None
        self._store = None
        self._controller = None

    def __eq__(self, other):
        assert isinstance(other, Column)
        return (self._col_num == other._col_num) and \
                (self._pattern_id == other._pattern_id)

    def __ne__(self, other):
        return not (self == other)

    def set_controller(self, controller):
        self._controller = controller
        self._store = controller.get_store()

    def get_trigger_row_positions(self):
        self._build_trigger_rows()
        return self._trigger_rows.keys()

    def get_trigger_row_positions_in_range(self, start, stop):
        self._build_trigger_rows()
        return [ts for ts in self._trigger_rows.keys() if start <= ts < stop]

    def get_trigger_count_at_row(self, row_ts):
        self._build_trigger_rows()
        return len(self._trigger_rows[row_ts])

    def get_trigger(self, row_ts, trigger_index):
        self._build_trigger_rows()
        return self._trigger_rows[row_ts][trigger_index]

    def has_trigger(self, row_ts, trigger_index):
        try:
            self.get_trigger(row_ts, trigger_index)
            return True
        except (KeyError, IndexError):
            return False

    def insert_trigger(self, row_ts, trigger_index, trigger):
        self._build_trigger_rows()

        if row_ts not in self._trigger_rows:
            self._trigger_rows[row_ts] = []

        self._trigger_rows[row_ts].insert(trigger_index, trigger)

        self._update_raw_data()

    def remove_trigger(self, row_ts, trigger_index):
        self._build_trigger_rows()
        assert self.has_trigger(row_ts, trigger_index)

        del self._trigger_rows[row_ts][trigger_index]

        self._update_raw_data()

    def shift_triggers_down(self, from_ts, add_ts):
        self._build_trigger_rows()
        assert add_ts >= 0

        huge_ts = tstamp.Tstamp(2**64)
        positions = self.get_trigger_row_positions_in_range(from_ts, huge_ts)
        if not positions:
            return False

        positions = reversed(sorted(positions))

        for row_ts in positions:
            new_ts = row_ts + add_ts
            assert new_ts not in self._trigger_rows
            row = self._trigger_rows.pop(row_ts)
            self._trigger_rows[new_ts] = row

        self._update_raw_data()
        return True

    def get_distance_to_next_trigger(self, from_ts):
        huge_ts = tstamp.Tstamp(2**64)
        positions = self.get_trigger_row_positions_in_range(from_ts, huge_ts)
        if not positions:
            return huge_ts
        return min(positions) - from_ts

    def shift_triggers_up(self, from_ts, remove_ts):
        self._build_trigger_rows()
        assert remove_ts >= 0

        huge_ts = tstamp.Tstamp(2**64)
        positions = self.get_trigger_row_positions_in_range(from_ts, huge_ts)
        if not positions:
            return False

        positions = sorted(positions)

        for row_ts in positions:
            new_ts = row_ts - remove_ts
            row = self._trigger_rows.pop(row_ts)
            if new_ts >= from_ts:
                assert new_ts not in self._trigger_rows
                self._trigger_rows[new_ts] = row

        self._update_raw_data()
        return True

    def _update_raw_data(self):
        raw_data = self._make_raw_data(self._trigger_rows)
        self._store[self._get_key()] = raw_data

    def _build_trigger_rows(self):
        if self._trigger_rows == None:
            self._trigger_rows = {}
            trigger_list = self._get_raw_data()
            for ts_data, evspec in trigger_list:
                ts = tstamp.Tstamp(ts_data)
                if ts not in self._trigger_rows:
                    self._trigger_rows[ts] = []
                trigger_type, argument = evspec
                trigger = Trigger(trigger_type, argument)
                self._trigger_rows[ts].append(trigger)

    def _get_key(self):
        key = '{}/col_{:02x}/p_triggers.json'.format(
                self._pattern_id, self._col_num)
        return key

    def _get_raw_data(self):
        key = self._get_key()
        try:
            triggers = self._store[key]
            return triggers
        except KeyError:
            return []

    def _make_raw_data(self, trigger_rows):
        raw_data = []
        for (ts, triggers) in trigger_rows.iteritems():
            for trigger in triggers:
                evspec = [trigger.get_type(), trigger.get_argument()]
                raw_data.append([tuple(ts), evspec])

        return raw_data


