# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2015-2016
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

import math
from types import NoneType

import tstamp
from gridpattern import STYLE_COUNT
from pattern import Pattern


class Grid():

    def __init__(self):
        self._controller = None
        self._store = None
        self._ui_model = None

        self._cached_grid_patterns = {}
        self._cached_allowed_line_styles = {}

    def set_controller(self, controller):
        self._controller = controller
        self._store = controller.get_store()

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model

    def _get_allowed_line_styles(self, spec, tr_height_ts):
        allowed_styles = set(xrange(STYLE_COUNT))

        for style in xrange(1, STYLE_COUNT):
            min_line_dist = spec['min_style_spacing'][style] * tr_height_ts
            first_ts = None
            prev_ts = None

            for line in spec['lines']:
                line_ts, line_style = line

                # Ignore disallowed lines or lines of lower priority
                if (line_style not in allowed_styles) or (line_style > style):
                    continue

                # Disallow style if too close to another checked line
                if (prev_ts != None) and ((line_ts - prev_ts) < min_line_dist):
                    allowed_styles.discard(style)
                    break

                if first_ts == None:
                    first_ts = line_ts
                prev_ts = line_ts
            else:
                if first_ts != None:
                    # Check wrap-around distance
                    next_ts = first_ts + spec['length']
                    if (next_ts - prev_ts) < min_line_dist:
                        allowed_styles.discard(style)

        return allowed_styles

    def _get_grid_spec(self, gp_id, tr_height_ts):
        if gp_id == None:
            gp_id = u'0'
        assert isinstance(gp_id, unicode)

        if gp_id in self._cached_grid_patterns:
            gp = self._cached_grid_patterns[gp_id]
        else:
            grid_manager = self._ui_model.get_grid_manager()
            gp = grid_manager.get_grid_pattern(gp_id)
            self._cached_grid_patterns[gp_id] = gp

        gp_length = gp.get_length()
        gp_lines = gp.get_lines()
        gp_style_spacing = [gp.get_line_style_spacing(i) for i in xrange(STYLE_COUNT)]

        spec = {
            'length': gp_length,
            'lines': gp_lines,
            'min_style_spacing': gp_style_spacing,
            'cycles_per_line': 1,
        }

        # Filter out line styles with insufficient separation
        if (gp_id, tr_height_ts) in self._cached_allowed_line_styles:
            allowed_styles = self._cached_allowed_line_styles[(gp_id, tr_height_ts)]
        else:
            allowed_styles = self._get_allowed_line_styles(spec, tr_height_ts)
            self._cached_allowed_line_styles[(gp_id, tr_height_ts)] = allowed_styles
        filtered_lines = [line for line in gp_lines if line[1] in allowed_styles]
        spec['lines'] = filtered_lines

        top_min_line_dist = spec['min_style_spacing'][0] * tr_height_ts
        if spec['length'] < top_min_line_dist:
            # Remove some top-level lines as they are too close to one another
            cycles_per_line = int(2**math.ceil(
                math.log(top_min_line_dist / float(spec['length']), 2)))
            assert cycles_per_line > 1
            spec['cycles_per_line'] = cycles_per_line

        return spec

    def _get_pattern(self, pat_num):
        pattern_id = 'pat_{:03x}'.format(pat_num)
        pattern = Pattern(pattern_id)
        pattern.set_controller(self._controller)
        return pattern

    def _get_base_grid_pattern_id(self, pat_num):
        pattern = self._get_pattern(pat_num)
        return pattern.get_base_grid_pattern_id()

    def _get_next_or_current_line_info(self, pat_num, col_num, row_ts, tr_height_ts):
        gp_id = self._get_base_grid_pattern_id(pat_num)
        assert isinstance(gp_id, (NoneType, unicode))
        grid_spec = self._get_grid_spec(gp_id, tr_height_ts)

        grid_length = grid_spec['length']
        grid_lines = grid_spec['lines']
        if grid_length == 0 or not grid_lines:
            return None

        gp_row_ts = row_ts % grid_length

        line_index = 0
        line_pat_ts = (row_ts +
                (grid_length + grid_lines[0][0] - gp_row_ts) % grid_length)
        for i, line in enumerate(grid_lines):
            line_ts, _ = line
            if line_ts >= gp_row_ts:
                line_index = i
                line_pat_ts = row_ts + line_ts - gp_row_ts
                break

        # Skip grid pattern cycles if we are zoomed far out
        cycles_per_line = grid_spec['cycles_per_line']
        gp_cycle_index = int(line_pat_ts // grid_length)
        gp_cycle_index_mod = gp_cycle_index % cycles_per_line
        if gp_cycle_index_mod != 0:
            skip_count = (cycles_per_line - gp_cycle_index_mod)
            line_pat_ts += skip_count * grid_length

        return line_index, line_pat_ts

    def get_next_or_current_line(self, pat_num, col_num, row_ts, tr_height_ts):
        line_info = self._get_next_or_current_line_info(
                pat_num, col_num, row_ts, tr_height_ts)
        if not line_info:
            return None

        gp_id = self._get_base_grid_pattern_id(pat_num)
        assert isinstance(gp_id, (NoneType, unicode))
        grid_spec = self._get_grid_spec(gp_id, tr_height_ts)

        line_index, line_pat_ts = line_info
        _, line_style = grid_spec['lines'][line_index]

        return line_pat_ts, line_style

    def get_next_line(self, pat_num, col_num, row_ts, tr_height_ts):
        next_ts = row_ts + tstamp.Tstamp(0, 1)
        return self.get_next_or_current_line(pat_num, col_num, next_ts, tr_height_ts)

    def get_prev_or_current_line(self, pat_num, col_num, row_ts, tr_height_ts):
        next_ts = row_ts - tstamp.Tstamp(0, 1)
        return self.get_prev_line(pat_num, col_num, next_ts, tr_height_ts)

    def get_prev_line(self, pat_num, col_num, row_ts, tr_height_ts):
        line_info = self._get_next_or_current_line_info(
                pat_num, col_num, row_ts, tr_height_ts)
        if not line_info:
            return None

        gp_id = self._get_base_grid_pattern_id(pat_num)
        assert isinstance(gp_id, (NoneType, unicode))
        grid_spec = self._get_grid_spec(gp_id, tr_height_ts)
        grid_length = grid_spec['length']
        grid_lines = grid_spec['lines']

        next_line_index, next_line_pat_ts = line_info
        line_index = (len(grid_lines) + next_line_index - 1) % len(grid_lines)

        line_ts, line_style = grid_lines[line_index]

        next_line_ts, _ = grid_lines[next_line_index]
        ts_to_next_line = next_line_ts - line_ts
        if next_line_index <= line_index:
            ts_to_next_line += grid_length

        line_pat_ts = next_line_pat_ts - ts_to_next_line

        # Skip grid pattern_cycles if we are zoomed far out
        cycles_per_line = grid_spec['cycles_per_line']
        gp_cycle_index = int(line_pat_ts // grid_length)
        gp_cycle_index_mod = gp_cycle_index % cycles_per_line
        if gp_cycle_index_mod != 0:
            line_pat_ts -= gp_cycle_index_mod * grid_length

        return line_pat_ts, line_style

    def get_grid_lines(self, pat_num, col_num, start_ts, stop_ts, tr_height_ts):
        gp_id = self._get_base_grid_pattern_id(pat_num)
        assert isinstance(gp_id, (NoneType, unicode))
        grid_spec = self._get_grid_spec(gp_id, tr_height_ts)

        lines = []

        grid_length = grid_spec['length']
        grid_lines = grid_spec['lines']
        if grid_length > 0 and grid_lines:
            # Find our first line in the grid pattern
            line_index, line_pat_ts = self._get_next_or_current_line_info(
                    pat_num, col_num, start_ts, tr_height_ts)

            # Add lines from the grid pattern until stop_ts is reached
            skip_mod = 0
            while line_pat_ts < stop_ts:
                _, line_style = grid_lines[line_index]
                if skip_mod == 0:
                    lines.append((line_pat_ts, line_style))
                skip_mod = (skip_mod + 1) % grid_spec['cycles_per_line']

                next_line_index = (line_index + 1) % len(grid_lines)

                cur_line_ts, _ = grid_lines[line_index]
                next_line_ts, _ = grid_lines[next_line_index]
                ts_to_next_line = next_line_ts - cur_line_ts
                if next_line_index <= line_index:
                    ts_to_next_line += grid_length

                line_index = next_line_index
                line_pat_ts += ts_to_next_line

        return lines

