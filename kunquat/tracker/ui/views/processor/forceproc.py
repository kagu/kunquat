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

from kunquat.tracker.ui.qt import *

from kunquat.tracker.ui.views.envelope import Envelope
from .procnumslider import ProcNumSlider
from .proctimeenv import ProcessorTimeEnvelope
from .processorupdater import ProcessorUpdater


class ForceProc(QWidget, ProcessorUpdater):

    @staticmethod
    def get_name():
        return 'Force'

    def __init__(self):
        super().__init__()

        self._global_force = GlobalForceSlider()
        self._force_variation = ForceVarSlider()
        self._force_envelope = ForceEnvelope()
        self._ramp_release = RampReleaseToggle()
        self._force_release_envelope = ForceReleaseEnvelope()

        self.add_to_updaters(
                self._global_force,
                self._force_variation,
                self._force_envelope,
                self._ramp_release,
                self._force_release_envelope)

        self._sliders_layout = QGridLayout()
        self._sliders_layout.setContentsMargins(0, 0, 0, 0)
        self._sliders_layout.setVerticalSpacing(0)
        self._sliders_layout.addWidget(QLabel('Global force:'), 0, 0)
        self._sliders_layout.addWidget(self._global_force, 0, 1)
        self._sliders_layout.addWidget(QLabel('Force variation:'), 1, 0)
        self._sliders_layout.addWidget(self._force_variation, 1, 1)

        v = QVBoxLayout()
        v.setContentsMargins(4, 4, 4, 4)
        v.setSpacing(4)
        v.addLayout(self._sliders_layout)
        v.addWidget(self._force_envelope)
        v.addWidget(self._ramp_release)
        v.addWidget(self._force_release_envelope)
        self.setLayout(v)

    def _on_setup(self):
        self.register_action('signal_style_changed', self._update_style)
        self._update_style()

    def _update_style(self):
        style_mgr = self._ui_model.get_style_manager()

        self._sliders_layout.setHorizontalSpacing(
                style_mgr.get_scaled_size_param('large_padding'))

        margin = style_mgr.get_scaled_size_param('medium_padding')
        self.layout().setContentsMargins(margin, margin, margin, margin)
        self.layout().setSpacing(style_mgr.get_scaled_size_param('medium_padding'))


class ForceNumSlider(ProcNumSlider):

    def __init__(self, decimals, min_value, max_value):
        super().__init__(decimals, min_value, max_value, title='', width_txt='-00.0')

    def _get_force_params(self):
        module = self._ui_model.get_module()
        au = module.get_audio_unit(self._au_id)
        proc = au.get_processor(self._proc_id)
        force_params = proc.get_type_params()
        return force_params


class GlobalForceSlider(ForceNumSlider):

    def __init__(self):
        super().__init__(1, -64.0, 18.0)

    def _get_update_signal_type(self):
        return '_'.join(('signal_force_global_force', self._proc_id))

    def _update_value(self):
        force_params = self._get_force_params()
        self.set_number(force_params.get_global_force())

    def _value_changed(self, value):
        force_params = self._get_force_params()
        force_params.set_global_force(value)
        self._updater.signal_update(self._get_update_signal_type())


class ForceVarSlider(ForceNumSlider):

    def __init__(self):
        super().__init__(1, 0.0, 32.0)

    def _get_update_signal_type(self):
        return '_'.join(('signal_force_force_variation', self._proc_id))

    def _update_value(self):
        force_params = self._get_force_params()
        self.set_number(force_params.get_force_variation())

    def _value_changed(self, value):
        force_params = self._get_force_params()
        force_params.set_force_variation(value)
        self._updater.signal_update(self._get_update_signal_type())


class RampReleaseToggle(QCheckBox, ProcessorUpdater):

    def __init__(self):
        super().__init__()
        self.setText('Ramp release')

    def _on_setup(self):
        self.register_action(self._get_update_signal_type(), self._update_state)
        self.register_action(
                'signal_force_release_envelope_{}'.format(self._proc_id),
                self._update_state)

        self.stateChanged.connect(self._state_changed)

        self._update_state()

    def _get_update_signal_type(self):
        return '_'.join(('signal_force_ramp_release', self._proc_id))

    def _get_force_params(self):
        module = self._ui_model.get_module()
        au = module.get_audio_unit(self._au_id)
        proc = au.get_processor(self._proc_id)
        force_params = proc.get_type_params()
        return force_params

    def _update_state(self):
        force_params = self._get_force_params()
        ramp_enabled = force_params.get_release_ramp_enabled()
        release_env_enabled = force_params.get_release_envelope_enabled()

        old_block = self.blockSignals(True)
        self.setCheckState(Qt.Checked if ramp_enabled else Qt.Unchecked)
        self.setEnabled(not release_env_enabled)
        self.blockSignals(old_block)

    def _state_changed(self, new_state):
        enabled = (new_state == Qt.Checked)
        force_params = self._get_force_params()
        force_params.set_release_ramp_enabled(enabled)
        self._updater.signal_update(self._get_update_signal_type())


class ForceEnvelopeBase(ProcessorTimeEnvelope):

    def __init__(self):
        super().__init__()

    def _get_force_params(self):
        module = self._ui_model.get_module()
        au = module.get_audio_unit(self._au_id)
        proc = au.get_processor(self._proc_id)
        force_params = proc.get_type_params()
        return force_params


class ForceEnvelope(ForceEnvelopeBase):

    def __init__(self):
        super().__init__()

    def _get_title(self):
        return 'Force envelope'

    def _allow_loop(self):
        return True

    def _make_envelope_widget(self):
        envelope = Envelope({ 'enable_zoom_x': True })
        ev = envelope.get_envelope_view()
        ev.set_node_count_max(32)
        ev.set_y_range(0, 1)
        ev.set_x_range(0, 4)
        ev.set_first_lock(True, False)
        ev.set_x_range_adjust(False, True)
        return envelope

    def _get_update_signal_type(self):
        return '_'.join(('signal_force_envelope', self._proc_id))

    def _get_enabled(self):
        return self._get_force_params().get_envelope_enabled()

    def _set_enabled(self, enabled):
        self._get_force_params().set_envelope_enabled(enabled)

    def _get_loop_enabled(self):
        return self._get_force_params().get_envelope_loop_enabled()

    def _set_loop_enabled(self, enabled):
        self._get_force_params().set_envelope_loop_enabled(enabled)

    def _get_envelope_data(self):
        return self._get_force_params().get_envelope()

    def _set_envelope_data(self, envelope):
        self._get_force_params().set_envelope(envelope)


class ForceReleaseEnvelope(ForceEnvelopeBase):

    def __init__(self):
        super().__init__()

    def _get_title(self):
        return 'Force release envelope'

    def _allow_loop(self):
        return False

    def _make_envelope_widget(self):
        envelope = Envelope({ 'enable_zoom_x': True })
        ev = envelope.get_envelope_view()
        ev.set_node_count_max(32)
        ev.set_y_range(0, 1)
        ev.set_x_range(0, 4)
        ev.set_first_lock(True, False)
        ev.set_last_lock(False, True)
        ev.set_x_range_adjust(False, True)
        return envelope

    def _get_update_signal_type(self):
        return '_'.join(('signal_force_release_envelope', self._proc_id))

    def _get_enabled(self):
        return self._get_force_params().get_release_envelope_enabled()

    def _set_enabled(self, enabled):
        self._get_force_params().set_release_envelope_enabled(enabled)

    def _get_envelope_data(self):
        return self._get_force_params().get_release_envelope()

    def _set_envelope_data(self, envelope):
        self._get_force_params().set_release_envelope(envelope)


