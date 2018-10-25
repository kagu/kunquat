# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2016-2018
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

import kunquat.tracker.config as config


class StyleManager():

    _STYLE_DEFAULTS = {
        'def_font_size'                     : 0,
        'def_font_family'                   : '',
        'border_contrast'                   : 0.25,
        'button_brightness'                 : 0.10,
        'button_press_brightness'           : -0.2,

        'border_thick_radius'               : 0.4,
        'border_thick_width'                : 0.2,
        'border_thin_radius'                : 0.2,
        'border_thin_width'                 : 0.1,
        'combobox_arrow_size'               : 0.8,
        'combobox_button_size'              : 0.6,
        'dialog_button_width'               : 9.0,
        'dialog_icon_size'                  : 7.0,
        'large_padding'                     : 0.8,
        'medium_padding'                    : 0.4,
        'menu_arrow_size'                   : 1.0,
        'radio_border_radius'               : 0.499,
        'radio_check_size'                  : 1.0,
        'radio_check_spacing'               : 0.5,
        'scrollbar_margin'                  : 0.25,
        'scrollbar_size'                    : 1.2,
        'slider_handle_size'                : 3.0,
        'slider_thickness'                  : 1.0,
        'small_padding'                     : 0.2,
        'tab_bar_margin'                    : 0.4,
        'tiny_arrow_button_size'            : 0.9,
        'tiny_padding'                      : 0.1,
        'tool_button_size'                  : 3.4,
        'tool_button_padding'               : 0.62,
        'typewriter_button_size'            : 5.6,
        'typewriter_padding'                : 2.9,

        'bg_colour'                         : '#4c474e',
        'fg_colour'                         : '#dbdbdb',
        'bg_sunken_colour'                  : '#2b2238',
        'disabled_fg_colour'                : '#969696',
        'important_button_bg_colour'        : '#d5412c',
        'important_button_fg_colour'        : '#fff',
        'active_indicator_colour'           : '#ff2020',
        'conns_bg_colour'                   : '#111',
        'conns_edge_colour'                 : '#ccc',
        'conns_focus_colour'                : '#f72',
        'conns_port_colour'                 : '#eca',
        'conns_invalid_port_colour'         : '#f33',
        'conns_inst_bg_colour'              : '#335',
        'conns_inst_fg_colour'              : '#def',
        'conns_effect_bg_colour'            : '#543',
        'conns_effect_fg_colour'            : '#fed',
        'conns_proc_voice_bg_colour'        : '#255',
        'conns_proc_voice_fg_colour'        : '#cff',
        'conns_proc_voice_selected_colour'  : '#9b9',
        'conns_proc_mixed_bg_colour'        : '#525',
        'conns_proc_mixed_fg_colour'        : '#fcf',
        'conns_master_bg_colour'            : '#353',
        'conns_master_fg_colour'            : '#dfd',
        'envelope_bg_colour'                : '#000',
        'envelope_axis_label_colour'        : '#ccc',
        'envelope_axis_line_colour'         : '#ccc',
        'envelope_curve_colour'             : '#68a',
        'envelope_node_colour'              : '#eca',
        'envelope_focus_colour'             : '#f72',
        'envelope_loop_marker_colour'       : '#7799bb',
        'peak_meter_bg_colour'              : '#000',
        'peak_meter_low_colour'             : '#191',
        'peak_meter_mid_colour'             : '#ddcc33',
        'peak_meter_high_colour'            : '#e21',
        'peak_meter_clip_colour'            : '#f32',
        'position_bg_colour'                : '#000',
        'position_fg_colour'                : '#6d6',
        'position_stopped_colour'           : '#555',
        'position_play_colour'              : '#6d6',
        'position_record_colour'            : '#d43',
        'position_infinite_colour'          : '#fd5',
        'position_title_colour'             : '#777',
        'sample_map_bg_colour'              : '#000',
        'sample_map_axis_label_colour'      : '#ccc',
        'sample_map_axis_line_colour'       : '#ccc',
        'sample_map_focus_colour'           : '#f72',
        'sample_map_point_colour'           : '#eca',
        'sample_map_selected_colour'        : '#ffd',
        'sheet_area_selection_colour'       : '#8ac',
        'sheet_canvas_bg_colour'            : '#111',
        'sheet_column_bg_colour'            : '#000',
        'sheet_column_border_colour'        : '#222',
        'sheet_cursor_view_line_colour'     : '#def',
        'sheet_cursor_edit_line_colour'     : '#f84',
        'sheet_grid_level_1_colour'         : '#aaa',
        'sheet_grid_level_2_colour'         : '#666',
        'sheet_grid_level_3_colour'         : '#444',
        'sheet_header_bg_colour'            : '#242',
        'sheet_header_fg_colour'            : '#cea',
        'sheet_header_solo_colour'          : '#7e6',
        'sheet_ruler_bg_colour'             : '#125',
        'sheet_ruler_fg_colour'             : '#acf',
        'sheet_ruler_playback_marker_colour': '#d68',
        'sheet_playback_cursor_colour'      : '#6e6',
        'sheet_trigger_default_colour'      : '#cde',
        'sheet_trigger_note_on_colour'      : '#fdb',
        'sheet_trigger_hit_colour'          : '#be8',
        'sheet_trigger_note_off_colour'     : '#c96',
        'sheet_trigger_warning_bg_colour'   : '#e31',
        'sheet_trigger_warning_fg_colour'   : '#ffc',
        'system_load_bg_colour'             : '#000',
        'system_load_low_colour'            : '#191',
        'system_load_mid_colour'            : '#dc3',
        'system_load_high_colour'           : '#e21',
        'text_bg_colour'                    : '#211d2c',
        'text_fg_colour'                    : '#e9d0a7',
        'text_selected_bg_colour'           : '#36a',
        'text_selected_fg_colour'           : '#ffc',
        'text_disabled_fg_colour'           : '#8b7865',
        'waveform_bg_colour'                : '#000',
        'waveform_focus_colour'             : '#fa5',
        'waveform_centre_line_colour'       : '#666',
        'waveform_zoomed_out_colour'        : '#67d091',
        'waveform_single_item_colour'       : '#67d091',
        'waveform_interpolated_colour'      : '#396',
        'waveform_loop_marker_colour'       : '#dfd58e',
    }

    def __init__(self):
        self._controller = None
        self._ui_model = None
        self._session = None
        self._share = None
        self._init_ss = None

    def set_controller(self, controller):
        self._controller = controller
        self._session = controller.get_session()
        self._share = controller.get_share()

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model

    def set_init_style_sheet(self, init_ss):
        self._init_ss = init_ss

    def get_init_style_sheet(self):
        return self._init_ss

    def get_style_sheet_template(self):
        return self._share.get_style_sheet('default')

    def get_icons_dir(self):
        return self._share.get_icons_dir()

    def set_custom_style_enabled(self, enabled):
        config_style = self._get_config_style()
        config_style['enabled'] = enabled
        self._set_config_style(config_style)

    def is_custom_style_enabled(self):
        config_style = self._get_config_style()
        return config_style.get('enabled', True)

    def get_style_param(self, key):
        config_style = self._get_config_style()
        return config_style.get(key, self._STYLE_DEFAULTS[key])

    def set_style_param(self, key, value):
        config_style = self._get_config_style()

        config_style[key] = value
        if key.endswith('_colour'):
            # Make sure that all colours are stored if one is changed
            for k in self._STYLE_DEFAULTS.keys():
                if (k != key) and (k not in config_style):
                    config_style[k] = self._STYLE_DEFAULTS[k]

        self._set_config_style(config_style)

    def set_reference_font_height(self, height):
        self._session.set_reference_font_height(height)

    def get_scaled_size(self, size_norm, min_size=1):
        ref_height = self._session.get_reference_font_height()
        return max(min_size, int(round(size_norm * ref_height)))

    def get_scaled_size_param(self, size_param, min_size=1):
        size_norm = self.get_style_param(size_param)
        return self.get_scaled_size(size_norm, min_size)

    def get_adjusted_colour(self, param, brightness):
        orig_colour = self._get_colour_from_str(self.get_style_param(param))
        adjusted_colour = (c + brightness for c in orig_colour)
        return self._get_str_from_colour(adjusted_colour)

    def get_link_colour(self):
        shift = (-0.3, 0.1, 0.6)
        fg_colour = self._get_colour_from_str(self.get_style_param('fg_colour'))
        return self._get_str_from_colour(c + s for (c, s) in zip(fg_colour, shift))

    def _get_colour_from_str(self, s):
        if len(s) == 4:
            cs = [s[1], s[2], s[3]]
            colour = tuple(int(c, 16) / 15 for c in cs)
        elif len(s) == 7:
            cs = [s[1:3], s[3:5], s[5:7]]
            colour = tuple(int(c, 16) / 255 for c in cs)
        else:
            assert False
        return colour

    def _get_str_from_colour(self, colour):
        clamped = [min(max(0, c), 1) for c in colour]
        cs = ['{:02x}'.format(int(c * 255)) for c in clamped]
        s = '#' + ''.join(cs)
        assert len(s) == 7
        return s

    def _set_config_style(self, style):
        cfg = config.get_config()
        cfg.set_value('style', style)

    def _get_config_style(self):
        cfg = config.get_config()
        return cfg.get_value('style') or {}


