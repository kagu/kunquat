

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010-2015
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <stdlib.h>
#include <stdio.h>

#include <debug/assert.h>
#include <devices/Generator.h>
#include <devices/generators/Generator_common.h>
#include <Filter.h>
#include <kunquat/limits.h>
#include <mathnum/common.h>
#include <player/Work_buffers.h>


#define RAMP_ATTACK_TIME (500.0)
#define RAMP_RELEASE_TIME (200.0)


void Generator_common_handle_pitch(
        const Generator* gen,
        Voice_state* vstate,
        const Work_buffers* wbs,
        int32_t nframes,
        int32_t offset)
{
    assert(gen != NULL);
    assert(vstate != NULL);
    assert(wbs != NULL);
    (void)gen;

    const Work_buffer* wb_pitch_params = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_PITCH_PARAMS);
    const Work_buffer* wb_actual_pitches = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_ACTUAL_PITCHES);

    float new_pitch = vstate->pitch;
    float new_actual_pitch = vstate->actual_pitch;

    float* pitch_params = Work_buffer_get_contents_mut(wb_pitch_params);
    pitch_params[offset] = new_pitch;
    ++pitch_params;

    float* actual_pitches = Work_buffer_get_contents_mut(wb_actual_pitches);
    actual_pitches[offset] = new_actual_pitch;
    ++actual_pitches;

    for (int32_t i = offset; i < nframes; ++i)
    {
        // Update pitch slide
        if (Slider_in_progress(&vstate->pitch_slider))
            new_pitch = Slider_step(&vstate->pitch_slider);

        new_actual_pitch = new_pitch;

        if (vstate->arpeggio)
        {
            // Adjust actual pitch according to the current arpeggio state
            assert(!isnan(vstate->arpeggio_tones[0]));
            double diff = exp2(
                    (vstate->arpeggio_tones[vstate->arpeggio_note] -
                        vstate->arpeggio_ref) / 1200);
            new_actual_pitch *= diff;

            // Update arpeggio state
            vstate->arpeggio_frames += 1;
            if (vstate->arpeggio_frames >= vstate->arpeggio_length)
            {
                vstate->arpeggio_frames -= vstate->arpeggio_length;
                ++vstate->arpeggio_note;
                if (vstate->arpeggio_note > KQT_ARPEGGIO_NOTES_MAX ||
                        isnan(vstate->arpeggio_tones[vstate->arpeggio_note]))
                    vstate->arpeggio_note = 0;
            }
        }

        // Update vibrato
        if (LFO_active(&vstate->vibrato))
            new_actual_pitch *= LFO_step(&vstate->vibrato);

        pitch_params[i] = new_pitch;
        actual_pitches[i] = new_actual_pitch;
    }

    vstate->pitch = new_pitch;
    vstate->actual_pitch = new_actual_pitch;
    vstate->prev_actual_pitch = actual_pitches[nframes - 2];

    return;
}


#if 0
void Generator_common_handle_pitch(const Generator* gen, Voice_state* vstate)
{
    assert(gen != NULL);
    assert(vstate != NULL);
    (void)gen;

    if (Slider_in_progress(&vstate->pitch_slider))
        vstate->pitch = Slider_step(&vstate->pitch_slider);

    vstate->prev_actual_pitch = vstate->actual_pitch;
    vstate->actual_pitch = vstate->pitch;

#if 0
    if (gen->conf->pitch_lock_enabled)
    {
        vstate->pitch = vstate->actual_pitch = gen->conf->pitch_lock_freq;
        // TODO: The following alternative would enable a useful mode where
        //       the actual pitch is locked but other pitch-dependent mappings
        //       follow the original pitch.
//        vstate->actual_pitch = gen->conf->pitch_lock_freq;
    }
    else
#endif
    {
        if (vstate->arpeggio)
        {
            assert(!isnan(vstate->arpeggio_tones[0]));
            double diff = exp2(
                    (vstate->arpeggio_tones[vstate->arpeggio_note] -
                        vstate->arpeggio_ref) / 1200);
            vstate->actual_pitch *= diff;

#if 0
            if (vstate->arpeggio_note > 0)
            {
                vstate->actual_pitch *= vstate->arpeggio_factors[
                                       vstate->arpeggio_note - 1];
            }
#endif

            vstate->arpeggio_frames += 1;
            if (vstate->arpeggio_frames >= vstate->arpeggio_length)
            {
                vstate->arpeggio_frames -= vstate->arpeggio_length;
                ++vstate->arpeggio_note;
                if (vstate->arpeggio_note > KQT_ARPEGGIO_NOTES_MAX ||
                        isnan(vstate->arpeggio_tones[vstate->arpeggio_note]))
                    vstate->arpeggio_note = 0;
            }

#if 0
            if (vstate->arpeggio_note > 0)
            {
                fprintf(stderr, "%f %f %f %f\n", vstate->arpeggio_ref,
                        vstate->arpeggio_tones[0],
                        vstate->arpeggio_tones[vstate->arpeggio_note],
                        diff);
            }
#endif
        }

        if (LFO_active(&vstate->vibrato))
            vstate->actual_pitch *= LFO_step(&vstate->vibrato);
    }

    return;
}
#endif


int32_t Generator_common_handle_force(
        const Generator* gen,
        Ins_state* ins_state,
        Voice_state* vstate,
        const Work_buffers* wbs,
        uint32_t freq,
        int32_t nframes,
        int32_t offset)
{
    assert(gen != NULL);
    assert(ins_state != NULL);
    assert(vstate != NULL);
    assert(wbs != NULL);

    const Work_buffer* wb_actual_pitches = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_ACTUAL_PITCHES);
    const Work_buffer* wb_actual_forces = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_ACTUAL_FORCES);

    const float* actual_pitches = Work_buffer_get_contents(wb_actual_pitches) + 1;

    float new_actual_force = vstate->actual_force;

    float* actual_forces = Work_buffer_get_contents_mut(wb_actual_forces);
    actual_forces[offset] = new_actual_force;
    ++actual_forces;

    int32_t buf_stop = nframes;

    // Apply force slide & global force
    if (Slider_in_progress(&vstate->force_slider))
    {
        float new_force = vstate->force;
        for (int32_t i = offset; i < buf_stop; ++i)
        {
            new_force = Slider_step(&vstate->force_slider);
            actual_forces[i] = new_force * gen->ins_params->global_force;
        }
        vstate->force = new_force;
    }
    else
    {
        const float actual_force = vstate->force * gen->ins_params->global_force;
        for (int32_t i = offset; i < buf_stop; ++i)
            actual_forces[i] = actual_force;
    }

    // Apply tremolo
    if (LFO_active(&vstate->tremolo))
    {
        for (int32_t i = offset; i < buf_stop; ++i)
            actual_forces[i] *= LFO_step(&vstate->tremolo);
    }

    // Apply force envelope
    if (gen->ins_params->env_force_enabled)
    {
        const Envelope* env = gen->ins_params->env_force;

        const int32_t env_force_stop = Time_env_state_process(
                &vstate->force_env_state,
                env,
                gen->ins_params->env_force_scale_amount,
                gen->ins_params->env_force_center,
                0,
                1,
                wbs,
                offset,
                buf_stop,
                freq);

        const Work_buffer* wb_time_env = Work_buffers_get_buffer(
                wbs, WORK_BUFFER_TIME_ENV);
        float* time_env = Work_buffer_get_contents_mut(wb_time_env) + 1;

        if (vstate->force_env_state.is_finished)
        {
            const double* last_node = Envelope_get_node(
                    env, Envelope_node_count(env) - 1);
            const double last_value = last_node[1];
            if (last_value == 0)
            {
                buf_stop = env_force_stop;
            }
            else
            {
                // Fill the rest of the envelope buffer with the last value
                for (int32_t i = env_force_stop; i < buf_stop; ++i)
                    time_env[i] = last_value;
            }
        }

        for (int32_t i = offset; i < buf_stop; ++i)
            actual_forces[i] *= time_env[i];

        /*
        const int loop_start_index = Envelope_get_mark(env, 0);
        const int loop_end_index = Envelope_get_mark(env, 1);
        const double* loop_start =
            loop_start_index == -1 ? NULL :
            Envelope_get_node(env, loop_start_index);
        const double* loop_end =
            loop_end_index == -1 ? NULL :
            Envelope_get_node(env, loop_end_index);

        int32_t i = offset;
        for (; i < nframes; ++i)
        {
            const float actual_pitch = actual_pitches[i];
            const float prev_actual_pitch = actual_pitches[i - 1];

            if (gen->ins_params->env_force_scale_amount != 0 &&
                    actual_pitch != prev_actual_pitch)
            {
                vstate->fe_scale = pow(
                        actual_pitch / gen->ins_params->env_force_center,
                        gen->ins_params->env_force_scale_amount);
            }

            double* next_node = Envelope_get_node(env, vstate->fe_next_node);
            double scale = NAN;

            if (next_node == NULL)
            {
                //assert(loop_start == NULL);
                //assert(loop_end == NULL);
                double* last_node = Envelope_get_node(env,
                                            Envelope_node_count(env) - 1);
                scale = last_node[1];
            }
            else if (vstate->fe_pos >= next_node[0] || isnan(vstate->fe_update))
            {
                ++vstate->fe_next_node;
                if (loop_end_index >= 0 && loop_end_index < vstate->fe_next_node)
                {
                    assert(loop_start_index >= 0);
                    vstate->fe_next_node = loop_start_index;
                }
                scale = Envelope_get_value(env, vstate->fe_pos);
                if (isfinite(scale))
                {
                    double next_scale = Envelope_get_value(
                            env,
                            vstate->fe_pos + 1.0 / freq);
                    vstate->fe_value = scale;
                    vstate->fe_update = next_scale - scale;
                }
                else
                {
                    scale = Envelope_get_node(env, Envelope_node_count(env) - 1)[1];
                }
            }
            else
            {
                assert(isfinite(vstate->fe_update));
                vstate->fe_value += vstate->fe_update * vstate->fe_scale;
                scale = vstate->fe_value;
                if (scale < 0)
                    scale = 0;
            }

            // Apply envelope value
            assert(isfinite(scale));
            actual_forces[i] *= scale;

            // Update envelope position
            double new_pos = vstate->fe_pos + vstate->fe_scale / freq;

            if (loop_start != NULL && loop_end != NULL)
            {
                if (new_pos > loop_end[0])
                {
                    double loop_len = loop_end[0] - loop_start[0];
                    assert(loop_len >= 0);
                    if (loop_len == 0)
                    {
                        new_pos = loop_end[0];
                    }
                    else
                    {
                        double exceed = new_pos - loop_end[0];
                        double offset = fmod(exceed, loop_len);
                        new_pos = loop_start[0] + offset;
                        assert(new_pos >= loop_start[0]);
                        assert(new_pos <= loop_end[0]);
                        vstate->fe_next_node = loop_start_index;
                    }
                }
            }
            else
            {
                double* last = Envelope_get_node(env, Envelope_node_count(env) - 1);
                if (new_pos > last[0])
                {
                    new_pos = last[0];
                    if (vstate->fe_pos > last[0] && last[1] == 0)
                        break;
                }
            }
            vstate->fe_pos = new_pos;
        }
        // */
    }

    int32_t i = offset;
    for (; i < nframes; ++i)
    {
        const float actual_pitch = actual_pitches[i];
        const float prev_actual_pitch = actual_pitches[i - 1];

        float new_actual_force = actual_forces[i];

        if (!vstate->note_on)
        {
            if (gen->ins_params->env_force_rel_enabled)
            {
                if (gen->ins_params->env_force_rel_scale_amount != 0 &&
                        (actual_pitch != prev_actual_pitch ||
                         isnan(vstate->rel_fe_scale)))
                {
                    vstate->rel_fe_scale = pow(
                            actual_pitch /
                                gen->ins_params->env_force_rel_center,
                            gen->ins_params->env_force_rel_scale_amount);
                }
                else if (isnan(vstate->rel_fe_scale))
                {
                    vstate->rel_fe_scale = 1;
                }

                Envelope* env = gen->ins_params->env_force_rel;
                double* next_node = Envelope_get_node(env, vstate->rel_fe_next_node);
                if (next_node == NULL)
                {
                    // This may occur if the user removes nodes during playback
                    next_node = Envelope_get_node(env,
                                                  Envelope_node_count(env) - 1);
                    assert(next_node != NULL);
                }

                double scale = NAN;

                if (vstate->rel_fe_pos >= next_node[0])
                {
                    ++vstate->rel_fe_next_node;
                    scale = Envelope_get_value(env, vstate->rel_fe_pos);
                    if (!isfinite(scale))
                        break;

                    double next_scale = Envelope_get_value(
                            env,
                            vstate->rel_fe_pos + 1.0 / freq);
                    vstate->rel_fe_value = scale;
                    vstate->rel_fe_update = next_scale - scale;
                }
                else
                {
                    assert(isfinite(vstate->rel_fe_update));
                    vstate->rel_fe_value +=
                        vstate->rel_fe_update *
                        vstate->rel_fe_scale * (1.0 - ins_state->sustain);

                    scale = vstate->rel_fe_value;
                    if (scale < 0)
                        scale = 0;
                }

#if 0
                double scale = Envelope_get_value(gen->ins_params->env_force_rel,
                                                  vstate->rel_fe_pos);
                if (!isfinite(scale))
                {
                    vstate->active = false;

                    for (int i = 0; i < frame_count; ++i)
                        frames[i] = 0;

                    return;
                }
#endif

                vstate->rel_fe_pos +=
                    vstate->rel_fe_scale * (1.0 - ins_state->sustain) / freq;
                new_actual_force *= scale;
            }
        }

        actual_forces[i] = new_actual_force;
    }

    if (i < nframes)
        vstate->actual_force = 0;
    else
        vstate->actual_force = new_actual_force;

    return i;
}


#if 0
void Generator_common_handle_force(
        const Generator* gen,
        Ins_state* ins_state,
        Voice_state* vstate,
        double frames[],
        int frame_count,
        uint32_t freq)
{
    assert(gen != NULL);
    assert(ins_state != NULL);
    assert(vstate != NULL);
    assert(frames != NULL);
    assert(frame_count > 0);

    if (Slider_in_progress(&vstate->force_slider))
        vstate->force = Slider_step(&vstate->force_slider);

    vstate->actual_force = vstate->force * gen->ins_params->global_force;

    if (LFO_active(&vstate->tremolo))
        vstate->actual_force *= LFO_step(&vstate->tremolo);

    if (gen->ins_params->env_force_enabled)
    {
        Envelope* env = gen->ins_params->env_force;

        int loop_start_index = Envelope_get_mark(env, 0);
        int loop_end_index = Envelope_get_mark(env, 1);
        double* loop_start =
            loop_start_index == -1 ? NULL :
            Envelope_get_node(env, loop_start_index);
        double* loop_end =
            loop_end_index == -1 ? NULL :
            Envelope_get_node(env, loop_end_index);

        if (gen->ins_params->env_force_scale_amount != 0 &&
                vstate->actual_pitch != vstate->prev_actual_pitch)
        {
            vstate->fe_scale = pow(
                    vstate->actual_pitch / gen->ins_params->env_force_center,
                    gen->ins_params->env_force_scale_amount);
        }

        double* next_node = Envelope_get_node(env, vstate->fe_next_node);
        double scale = NAN;

        if (next_node == NULL)
        {
            //assert(loop_start == NULL);
            //assert(loop_end == NULL);
            double* last_node = Envelope_get_node(env,
                                        Envelope_node_count(env) - 1);
            scale = last_node[1];
        }
        else if (vstate->fe_pos >= next_node[0] || isnan(vstate->fe_update))
        {
            ++vstate->fe_next_node;
            if (loop_end_index >= 0 && loop_end_index < vstate->fe_next_node)
            {
                assert(loop_start_index >= 0);
                vstate->fe_next_node = loop_start_index;
            }
            scale = Envelope_get_value(env, vstate->fe_pos);
            if (isfinite(scale))
            {
                double next_scale = Envelope_get_value(
                        env,
                        vstate->fe_pos + 1.0 / freq);
                vstate->fe_value = scale;
                vstate->fe_update = next_scale - scale;
            }
            else
            {
                scale = Envelope_get_node(env, Envelope_node_count(env) - 1)[1];
            }
        }
        else
        {
            assert(isfinite(vstate->fe_update));
            vstate->fe_value += vstate->fe_update * vstate->fe_scale;
            scale = vstate->fe_value;
            if (scale < 0)
                scale = 0;
        }

//        double scale = Envelope_get_value(env, vstate->fe_pos);
        assert(isfinite(scale));
        vstate->actual_force *= scale;
        double new_pos = vstate->fe_pos + vstate->fe_scale / freq;

        if (loop_start != NULL && loop_end != NULL)
        {
            if (new_pos > loop_end[0])
            {
                double loop_len = loop_end[0] - loop_start[0];
                assert(loop_len >= 0);
                if (loop_len == 0)
                {
                    new_pos = loop_end[0];
                }
                else
                {
                    double exceed = new_pos - loop_end[0];
                    double offset = fmod(exceed, loop_len);
                    new_pos = loop_start[0] + offset;
                    assert(new_pos >= loop_start[0]);
                    assert(new_pos <= loop_end[0]);
                    vstate->fe_next_node = loop_start_index;
                }
            }
        }
        else
        {
            double* last = Envelope_get_node(env, Envelope_node_count(env) - 1);
            if (new_pos > last[0])
            {
                new_pos = last[0];
                if (vstate->fe_pos > last[0] && last[1] == 0)
                {
                    vstate->active = false;

                    for (int i = 0; i < frame_count; ++i)
                        frames[i] = 0;

                    return;
                }
            }
        }
        vstate->fe_pos = new_pos;
    }

    if (!vstate->note_on)
    {
        if (gen->ins_params->env_force_rel_enabled)
        {
            if (gen->ins_params->env_force_rel_scale_amount != 0 &&
                    (vstate->actual_pitch != vstate->prev_actual_pitch ||
                     isnan(vstate->rel_fe_scale)))
            {
                vstate->rel_fe_scale = pow(
                        vstate->actual_pitch /
                            gen->ins_params->env_force_rel_center,
                        gen->ins_params->env_force_rel_scale_amount);
            }
            else if (isnan(vstate->rel_fe_scale))
            {
                vstate->rel_fe_scale = 1;
            }

            Envelope* env = gen->ins_params->env_force_rel;
            double* next_node = Envelope_get_node(env, vstate->rel_fe_next_node);
            if (next_node == NULL)
            {
                // This may occur if the user removes nodes during playback
                next_node = Envelope_get_node(env,
                                              Envelope_node_count(env) - 1);
                assert(next_node != NULL);
            }

            double scale = NAN;

            if (vstate->rel_fe_pos >= next_node[0])
            {
                ++vstate->rel_fe_next_node;
                scale = Envelope_get_value(env, vstate->rel_fe_pos);
                if (!isfinite(scale))
                {
                    vstate->active = false;

                    for (int i = 0; i < frame_count; ++i)
                        frames[i] = 0;

                    return;
                }
                double next_scale = Envelope_get_value(
                        env,
                        vstate->rel_fe_pos + 1.0 / freq);
                vstate->rel_fe_value = scale;
                vstate->rel_fe_update = next_scale - scale;
            }
            else
            {
                assert(isfinite(vstate->rel_fe_update));
                vstate->rel_fe_value +=
                    vstate->rel_fe_update *
                    vstate->rel_fe_scale * (1.0 - ins_state->sustain);

                scale = vstate->rel_fe_value;
                if (scale < 0)
                    scale = 0;
            }

#if 0
            double scale = Envelope_get_value(gen->ins_params->env_force_rel,
                                              vstate->rel_fe_pos);
            if (!isfinite(scale))
            {
                vstate->active = false;

                for (int i = 0; i < frame_count; ++i)
                    frames[i] = 0;

                return;
            }
#endif

            vstate->rel_fe_pos +=
                vstate->rel_fe_scale * (1.0 - ins_state->sustain) / freq;
            vstate->actual_force *= scale;
        }
        else if (ins_state->sustain < 0.5)
        {
            if (vstate->ramp_release < 1)
            {
                for (int i = 0; i < frame_count; ++i)
                    frames[i] *= 1 - vstate->ramp_release;
            }
            else
            {
                vstate->active = false;

                for (int i = 0; i < frame_count; ++i)
                    frames[i] = 0;

                return;
            }

            vstate->ramp_release += RAMP_RELEASE_TIME / freq;
        }
    }

    for (int i = 0; i < frame_count; ++i)
        frames[i] *= vstate->actual_force;

    return;
}
#endif


void Generator_common_handle_filter(
        const Generator* gen,
        Voice_state* vstate,
        const Work_buffers* wbs,
        int ab_count,
        uint32_t freq,
        int32_t nframes,
        int32_t offset)
{
    assert(gen != NULL);
    assert(vstate != NULL);
    assert(wbs != NULL);

    const Work_buffer* wb_actual_forces = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_ACTUAL_FORCES);
    const Work_buffer* wb_audio_l = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_AUDIO_L);
    const Work_buffer* wb_audio_r = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_AUDIO_R);

    const float* actual_forces = Work_buffer_get_contents(wb_actual_forces) + 1;

    float* abufs[KQT_BUFFERS_MAX] =
    {
        Work_buffer_get_contents_mut(wb_audio_l),
        Work_buffer_get_contents_mut(wb_audio_r),
    };

    for (int32_t i = offset; i < nframes; ++i)
    {
        if (Slider_in_progress(&vstate->lowpass_slider))
            vstate->lowpass = Slider_step(&vstate->lowpass_slider);

        vstate->actual_lowpass = vstate->lowpass;

        if (LFO_active(&vstate->autowah))
            vstate->actual_lowpass *= LFO_step(&vstate->autowah);

        if (gen->ins_params->env_force_filter_enabled &&
                vstate->lowpass_xfade_pos >= 1)
        {
            double force = actual_forces[i];
            if (force > 1)
                force = 1;

            double factor = Envelope_get_value(
                    gen->ins_params->env_force_filter,
                    force);
            assert(isfinite(factor));
            vstate->actual_lowpass = min(vstate->actual_lowpass, 16384) * factor;
        }

        if (!vstate->lowpass_update &&
                vstate->lowpass_xfade_pos >= 1 &&
                (vstate->actual_lowpass < vstate->effective_lowpass * 0.98566319864018759 ||
                 vstate->actual_lowpass > vstate->effective_lowpass * 1.0145453349375237 ||
                 vstate->lowpass_resonance != vstate->effective_resonance))
        {
            vstate->lowpass_update = true;
            vstate->lowpass_xfade_state_used = vstate->lowpass_state_used;

            // TODO: figure out how to indicate start of note properly
            if ((vstate->pos > 0) || (i > offset))
                vstate->lowpass_xfade_pos = 0;
            else
                vstate->lowpass_xfade_pos = 1;

            vstate->lowpass_xfade_update = 200.0 / freq; // FIXME: / freq

            if (vstate->actual_lowpass < freq / 2)
            {
                int new_state = 1 - abs(vstate->lowpass_state_used);
                double lowpass = max(vstate->actual_lowpass, 1);
                two_pole_filter_create(lowpass / freq,
                        vstate->lowpass_resonance,
                        0,
                        vstate->lowpass_state[new_state].coeffs,
                        &vstate->lowpass_state[new_state].mul);
                for (int a = 0; a < KQT_BUFFERS_MAX; ++a)
                {
                    for (int k = 0; k < FILTER_ORDER; ++k)
                    {
                        vstate->lowpass_state[new_state].history1[a][k] = 0;
                        vstate->lowpass_state[new_state].history2[a][k] = 0;
                    }
                }
                vstate->lowpass_state_used = new_state;
    //            fprintf(stderr, "created filter with cutoff %f\n", vstate->actual_filter);
            }
            else
            {
                if (vstate->lowpass_state_used == -1)
                    vstate->lowpass_xfade_pos = 1;

                vstate->lowpass_state_used = -1;
            }

            vstate->effective_lowpass = vstate->actual_lowpass;
            vstate->effective_resonance = vstate->lowpass_resonance;
            vstate->lowpass_update = false;
        }

        if (vstate->lowpass_state_used > -1 || vstate->lowpass_xfade_state_used > -1)
        {
            assert(vstate->lowpass_state_used != vstate->lowpass_xfade_state_used);
            double result[KQT_BUFFERS_MAX] = { 0 };

            if (vstate->lowpass_state_used > -1)
            {
                Filter_state* fst =
                        &vstate->lowpass_state[vstate->lowpass_state_used];

                for (int a = 0; a < ab_count; ++a)
                {
                    result[a] = nq_zero_filter(
                            FILTER_ORDER,
                            fst->history1[a],
                            abufs[a][i]);
                    result[a] = iir_filter_strict_cascade(
                            FILTER_ORDER,
                            fst->coeffs,
                            fst->history2[a],
                            result[a]);
                    result[a] *= fst->mul;
                }
            }
            else
            {
                for (int a = 0; a < ab_count; ++a)
                    result[a] = abufs[a][i];
            }

            double vol = vstate->lowpass_xfade_pos;
            if (vol > 1)
                vol = 1;

            for (int a = 0; a < ab_count; ++a)
                result[a] *= vol;

            if (vstate->lowpass_xfade_pos < 1)
            {
                double fade_result[KQT_BUFFERS_MAX] = { 0 };

                if (vstate->lowpass_xfade_state_used > -1)
                {
                    Filter_state* fst =
                            &vstate->lowpass_state[vstate->lowpass_xfade_state_used];
                    for (int a = 0; a < ab_count; ++a)
                    {
                        fade_result[a] = nq_zero_filter(
                                FILTER_ORDER,
                                fst->history1[a],
                                abufs[a][i]);
                        fade_result[a] = iir_filter_strict_cascade(
                                FILTER_ORDER,
                                fst->coeffs,
                                fst->history2[a],
                                fade_result[a]);
                        fade_result[a] *= fst->mul;
                    }
                }
                else
                {
                    for (int a = 0; a < ab_count; ++a)
                        fade_result[a] = abufs[a][i];
                }

                double vol = 1 - vstate->lowpass_xfade_pos;
                if (vol > 0)
                {
                    for (int a = 0; a < ab_count; ++a)
                        result[a] += fade_result[a] * vol;
                }

                vstate->lowpass_xfade_pos += vstate->lowpass_xfade_update;
            }

            for (int a = 0; a < ab_count; ++a)
                abufs[a][i] = result[a];
        }
    }

    return;
}


#if 0
void Generator_common_handle_filter(
        const Generator* gen,
        Voice_state* vstate,
        double frames[],
        int frame_count,
        uint32_t freq)
{
    assert(gen != NULL);
    assert(vstate != NULL);
    assert(frames != NULL);
    assert(frame_count > 0);
    assert(freq > 0);

    if (Slider_in_progress(&vstate->lowpass_slider))
        vstate->lowpass = Slider_step(&vstate->lowpass_slider);

    vstate->actual_lowpass = vstate->lowpass;

    if (LFO_active(&vstate->autowah))
        vstate->actual_lowpass *= LFO_step(&vstate->autowah);

    if (gen->ins_params->env_force_filter_enabled &&
            vstate->lowpass_xfade_pos >= 1)
    {
        double force = vstate->actual_force;
        if (force > 1)
            force = 1;

        double factor = Envelope_get_value(
                gen->ins_params->env_force_filter,
                force);
        assert(isfinite(factor));
        vstate->actual_lowpass = min(vstate->actual_lowpass, 16384) * factor;
    }

    if (!vstate->lowpass_update &&
            vstate->lowpass_xfade_pos >= 1 &&
            (vstate->actual_lowpass < vstate->effective_lowpass * 0.98566319864018759 ||
             vstate->actual_lowpass > vstate->effective_lowpass * 1.0145453349375237 ||
             vstate->lowpass_resonance != vstate->effective_resonance))
    {
        vstate->lowpass_update = true;
        vstate->lowpass_xfade_state_used = vstate->lowpass_state_used;

        if (vstate->pos > 0)
            vstate->lowpass_xfade_pos = 0;
        else
            vstate->lowpass_xfade_pos = 1;

        vstate->lowpass_xfade_update = 200.0 / freq; // FIXME: / freq

        if (vstate->actual_lowpass < freq / 2)
        {
            int new_state = 1 - abs(vstate->lowpass_state_used);
            double lowpass = max(vstate->actual_lowpass, 1);
            two_pole_filter_create(lowpass / freq,
                    vstate->lowpass_resonance,
                    0,
                    vstate->lowpass_state[new_state].coeffs,
                    &vstate->lowpass_state[new_state].mul);
            for (int i = 0; i < KQT_BUFFERS_MAX; ++i)
            {
                for (int k = 0; k < FILTER_ORDER; ++k)
                {
                    vstate->lowpass_state[new_state].history1[i][k] = 0;
                    vstate->lowpass_state[new_state].history2[i][k] = 0;
                }
            }
            vstate->lowpass_state_used = new_state;
//            fprintf(stderr, "created filter with cutoff %f\n", vstate->actual_filter);
        }
        else
        {
            if (vstate->lowpass_state_used == -1)
                vstate->lowpass_xfade_pos = 1;

            vstate->lowpass_state_used = -1;
        }

        vstate->effective_lowpass = vstate->actual_lowpass;
        vstate->effective_resonance = vstate->lowpass_resonance;
        vstate->lowpass_update = false;
    }

    if (vstate->lowpass_state_used > -1 || vstate->lowpass_xfade_state_used > -1)
    {
        assert(vstate->lowpass_state_used != vstate->lowpass_xfade_state_used);
        double result[KQT_BUFFERS_MAX] = { 0 };

        if (vstate->lowpass_state_used > -1)
        {
            Filter_state* fst =
                    &vstate->lowpass_state[vstate->lowpass_state_used];

            for (int i = 0; i < frame_count; ++i)
            {
                result[i] = nq_zero_filter(
                        FILTER_ORDER,
                        fst->history1[i],
                        frames[i]);
                result[i] = iir_filter_strict_cascade(
                        FILTER_ORDER,
                        fst->coeffs,
                        fst->history2[i],
                        result[i]);
                result[i] *= fst->mul;
            }
        }
        else
        {
            for (int i = 0; i < frame_count; ++i)
                result[i] = frames[i];
        }

        double vol = vstate->lowpass_xfade_pos;
        if (vol > 1)
            vol = 1;

        for (int i = 0; i < frame_count; ++i)
            result[i] *= vol;

        if (vstate->lowpass_xfade_pos < 1)
        {
            double fade_result[KQT_BUFFERS_MAX] = { 0 };

            if (vstate->lowpass_xfade_state_used > -1)
            {
                Filter_state* fst =
                        &vstate->lowpass_state[vstate->lowpass_xfade_state_used];
                for (int i = 0; i < frame_count; ++i)
                {
                    fade_result[i] = nq_zero_filter(
                            FILTER_ORDER,
                            fst->history1[i],
                            frames[i]);
                    fade_result[i] = iir_filter_strict_cascade(
                            FILTER_ORDER,
                            fst->coeffs,
                            fst->history2[i],
                            fade_result[i]);
                    fade_result[i] *= fst->mul;
                }
            }
            else
            {
                for (int i = 0; i < frame_count; ++i)
                    fade_result[i] = frames[i];
            }

            double vol = 1 - vstate->lowpass_xfade_pos;
            if (vol > 0)
            {
                for (int i = 0; i < frame_count; ++i)
                    result[i] += fade_result[i] * vol;
            }

            vstate->lowpass_xfade_pos += vstate->lowpass_xfade_update;
        }

        for (int i = 0; i < frame_count; ++i)
            frames[i] = result[i];
    }

    return;
}
#endif


void Generator_common_ramp_attack(
        const Generator* gen,
        Voice_state* vstate,
        const Work_buffers* wbs,
        int ab_count,
        uint32_t freq,
        int32_t nframes,
        int32_t offset)
{
    assert(gen != NULL);
    assert(vstate != NULL);

    const Work_buffer* wb_audio_l = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_AUDIO_L);
    const Work_buffer* wb_audio_r = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_AUDIO_R);

    float* abufs[KQT_BUFFERS_MAX] =
    {
        Work_buffer_get_contents_mut(wb_audio_l),
        Work_buffer_get_contents_mut(wb_audio_r),
    };

    const float start_ramp_attack = vstate->ramp_attack;
    const float inc = RAMP_ATTACK_TIME / freq;

    for (int ch = 0; ch < ab_count; ++ch)
    {
        float ramp_attack = start_ramp_attack;

        for (int32_t i = offset; (i < nframes) && (ramp_attack < 1); ++i)
        {
            abufs[ch][i] *= ramp_attack;
            ramp_attack += inc;
        }

        vstate->ramp_attack = ramp_attack;
    }

    return;
}


#if 0
void Generator_common_ramp_attack(
        const Generator* gen,
        Voice_state* vstate,
        double frames[],
        int frame_count,
        uint32_t freq)
{
    assert(gen != NULL);
    assert(vstate != NULL);
    assert(frames != NULL);
    assert(frame_count > 0);
    assert(freq > 0);
    (void)gen;

    if (vstate->ramp_attack < 1)
    {
        for (int i = 0; i < frame_count; ++i)
            frames[i] *= vstate->ramp_attack;

        vstate->ramp_attack += RAMP_ATTACK_TIME / freq;
    }

    return;
}
#endif


int32_t Generator_common_ramp_release(
        const Generator* gen,
        const Ins_state* ins_state,
        Voice_state* vstate,
        const Work_buffers* wbs,
        int ab_count,
        uint32_t freq,
        int32_t nframes,
        int32_t offset)
{
    assert(gen != NULL);
    assert(vstate != NULL);
    assert(wbs != NULL);

    const bool do_ramp_release =
        !vstate->note_on &&
        ((vstate->ramp_release > 0) ||
            (!gen->ins_params->env_force_rel_enabled && (ins_state->sustain < 0.5)));

    if (do_ramp_release)
    {
        const Work_buffer* wb_audio_l = Work_buffers_get_buffer(
                wbs, WORK_BUFFER_AUDIO_L);
        const Work_buffer* wb_audio_r = Work_buffers_get_buffer(
                wbs, WORK_BUFFER_AUDIO_R);

        float* abufs[KQT_BUFFERS_MAX] =
        {
            Work_buffer_get_contents_mut(wb_audio_l),
            Work_buffer_get_contents_mut(wb_audio_r),
        };

        const float ramp_shift = RAMP_RELEASE_TIME / freq;
        const float ramp_start = vstate->ramp_release;
        float ramp = ramp_start;
        int32_t i = offset;

        for (int ch = 0; ch < ab_count; ++ch)
        {
            ramp = ramp_start;
            for (i = offset; (i < nframes) && (ramp < 1); ++i)
            {
                if (ramp < 1)
                    abufs[ch][i] *= 1 - ramp;

                ramp += ramp_shift;
            }
        }

        vstate->ramp_release = ramp;

        return i;
    }

    return nframes;
}


void Generator_common_handle_panning(
        const Generator* gen,
        Voice_state* vstate,
        const Work_buffers* wbs,
        int32_t nframes,
        int32_t offset)
{
    assert(gen != NULL);
    assert(vstate != NULL);
    assert(wbs != NULL);

    const Work_buffer* wb_pitch_params = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_PITCH_PARAMS);
    const Work_buffer* wb_audio_l = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_AUDIO_L);
    const Work_buffer* wb_audio_r = Work_buffers_get_buffer(
            wbs, WORK_BUFFER_AUDIO_R);

    const float* pitch_params = Work_buffer_get_contents(wb_pitch_params) + 1;
    float* audio_l = Work_buffer_get_contents_mut(wb_audio_l);
    float* audio_r = Work_buffer_get_contents_mut(wb_audio_r);

    for (int32_t i = offset; i < nframes; ++i)
    {
        if (Slider_in_progress(&vstate->panning_slider))
            vstate->panning = Slider_step(&vstate->panning_slider);

        vstate->actual_panning = vstate->panning;

        if (gen->ins_params->env_pitch_pan_enabled)
        {
            Envelope* env = gen->ins_params->env_pitch_pan;
            double cents = log2(pitch_params[i] / 440) * 1200;
            if (cents < -6000)
                cents = -6000;
            else if (cents > 6000)
                cents = 6000;

            double pan = Envelope_get_value(env, cents);
            assert(isfinite(pan));
            double separation = 1 - fabs(vstate->actual_panning);
            vstate->actual_panning += pan * separation;

            if (vstate->actual_panning < -1)
                vstate->actual_panning = -1;
            else if (vstate->actual_panning > 1)
                vstate->actual_panning = 1;
        }

        audio_l[i] *= 1 - vstate->actual_panning;
        audio_r[i] *= 1 + vstate->actual_panning;
    }

    return;
}


#if 0
void Generator_common_handle_panning(
        const Generator* gen,
        Voice_state* vstate,
        double frames[],
        int frame_count)
{
    assert(gen != NULL);
    assert(vstate != NULL);
    assert(frames != NULL);
    assert(frame_count > 0);

    if ((frame_count) >= 2)
    {
        if (Slider_in_progress(&vstate->panning_slider))
            vstate->panning = Slider_step(&vstate->panning_slider);

        vstate->actual_panning = vstate->panning;

        if (gen->ins_params->env_pitch_pan_enabled)
        {
            Envelope* env = gen->ins_params->env_pitch_pan;
            double cents = log2(vstate->pitch / 440) * 1200;
            if (cents < -6000)
                cents = -6000;
            else if (cents > 6000)
                cents = 6000;

            double pan = Envelope_get_value(env, cents);
            assert(isfinite(pan));
            double separation = 1 - fabs(vstate->actual_panning);
            vstate->actual_panning += pan * separation;

            if (vstate->actual_panning < -1)
                vstate->actual_panning = -1;
            else if (vstate->actual_panning > 1)
                vstate->actual_panning = 1;
        }

        frames[0] *= 1 - vstate->actual_panning;
        frames[1] *= 1 + vstate->actual_panning;
    }

    return;
}
#endif


