

/*
 * Author: Tomi Jylhä-Ollila, Finland 2015-2018
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <player/devices/processors/Gaincomp_state.h>

#include <debug/assert.h>
#include <init/devices/Device.h>
#include <init/devices/processors/Proc_gaincomp.h>
#include <mathnum/common.h>
#include <memory.h>
#include <player/devices/Device_state.h>
#include <player/devices/Device_thread_state.h>
#include <player/devices/Proc_state.h>
#include <player/devices/processors/Proc_state_utils.h>
#include <player/devices/Voice_state.h>

#include <stdint.h>


static void distort(
        const Proc_gaincomp* gc,
        const Work_buffer* in_buffer,
        Work_buffer* out_buffer,
        int32_t buf_start,
        int32_t buf_stop)
{
    rassert(gc != NULL);
    rassert(in_buffer != NULL);
    rassert(out_buffer != NULL);
    rassert(buf_start >= 0);
    rassert(buf_stop >= 0);

    if (gc->is_map_enabled && (gc->map != NULL))
    {
        const float* in_values = Work_buffer_get_contents(in_buffer, 0);
        float* out_values = Work_buffer_get_contents_mut(out_buffer, 0);

        const double* first = Envelope_get_node(gc->map, 0);
        if (first[0] == -1)
        {
            // Asymmetric distortion
            for (int32_t i = buf_start; i < buf_stop; ++i)
            {
                const float clamped_in_value = clamp(in_values[i], -1.0f, 1.0f);
                out_values[i] = (float)Envelope_get_value(gc->map, clamped_in_value);
            }
        }
        else
        {
            // Symmetric distortion
            for (int32_t i = buf_start; i < buf_stop; ++i)
            {
                const float in_value = in_values[i];
                const float abs_value = fabsf(in_value);

                float out_value = (float)Envelope_get_value(gc->map, min(abs_value, 1));
                if (in_value < 0)
                    out_value = -out_value;

                out_values[i] = out_value;
            }
        }
    }
    else
    {
        Work_buffer_copy(out_buffer, 0, in_buffer, 0, buf_start, buf_stop);
    }

    return;
}


enum
{
    PORT_IN_AUDIO_L = 0,
    PORT_IN_AUDIO_R,
    PORT_IN_COUNT
};

enum
{
    PORT_OUT_AUDIO_L = 0,
    PORT_OUT_AUDIO_R,
    PORT_OUT_COUNT
};


static void Gaincomp_pstate_render_mixed(
        Device_state* dstate,
        Device_thread_state* proc_ts,
        const Work_buffers* wbs,
        int32_t buf_start,
        int32_t buf_stop,
        double tempo)
{
    rassert(dstate != NULL);
    rassert(proc_ts != NULL);
    rassert(wbs != NULL);
    rassert(buf_start >= 0);
    rassert(tempo > 0);

    // Get input
    Work_buffer* in_buffers[] =
    {
        Device_thread_state_get_mixed_buffer(
                proc_ts, DEVICE_PORT_TYPE_RECV, PORT_IN_AUDIO_L),
        Device_thread_state_get_mixed_buffer(
                proc_ts, DEVICE_PORT_TYPE_RECV, PORT_IN_AUDIO_R),
    };

    // Get output
    Work_buffer* out_buffers[] =
    {
        Device_thread_state_get_mixed_buffer(
                proc_ts, DEVICE_PORT_TYPE_SEND, PORT_OUT_AUDIO_L),
        Device_thread_state_get_mixed_buffer(
                proc_ts, DEVICE_PORT_TYPE_SEND, PORT_OUT_AUDIO_R),
    };

    // Distort the signal
    const Proc_gaincomp* gc = (const Proc_gaincomp*)dstate->device->dimpl;
    for (int ch = 0; ch < 2; ++ch)
    {
        if ((in_buffers[ch] != NULL) && (out_buffers[ch] != NULL))
            distort(gc, in_buffers[ch], out_buffers[ch], buf_start, buf_stop);
    }

    return;
}


Device_state* new_Gaincomp_pstate(
        const Device* device, int32_t audio_rate, int32_t audio_buffer_size)
{
    rassert(device != NULL);
    rassert(audio_rate > 0);
    rassert(audio_buffer_size >= 0);

    Proc_state* proc_state =
        new_Proc_state_default(device, audio_rate, audio_buffer_size);
    if (proc_state == NULL)
        return NULL;

    proc_state->render_mixed = Gaincomp_pstate_render_mixed;

    return (Device_state*)proc_state;
}


int32_t Gaincomp_vstate_get_size(void)
{
    return 0;
}


int32_t Gaincomp_vstate_render_voice(
        Voice_state* vstate,
        Proc_state* proc_state,
        const Device_thread_state* proc_ts,
        const Au_state* au_state,
        const Work_buffers* wbs,
        int32_t buf_start,
        int32_t buf_stop,
        double tempo)
{
    rassert(vstate == NULL);
    rassert(proc_state != NULL);
    rassert(proc_ts != NULL);
    rassert(au_state != NULL);
    rassert(wbs != NULL);
    rassert(buf_start >= 0);
    rassert(buf_stop >= 0);
    rassert(isfinite(tempo));
    rassert(tempo > 0);

    // Get input
    Work_buffer* in_buffers[] =
    {
        Device_thread_state_get_voice_buffer(
                proc_ts, DEVICE_PORT_TYPE_RECV, PORT_IN_AUDIO_L),
        Device_thread_state_get_voice_buffer(
                proc_ts, DEVICE_PORT_TYPE_RECV, PORT_IN_AUDIO_R),
    };
    if ((in_buffers[0] == NULL) && (in_buffers[1] == NULL))
        return buf_start;

    // Get output
    Work_buffer* out_buffers[] =
    {
        Device_thread_state_get_voice_buffer(
                proc_ts, DEVICE_PORT_TYPE_SEND, PORT_OUT_AUDIO_L),
        Device_thread_state_get_voice_buffer(
                proc_ts, DEVICE_PORT_TYPE_SEND, PORT_OUT_AUDIO_R),
    };

    // Distort the signal
    const Proc_gaincomp* gc = (const Proc_gaincomp*)proc_state->parent.device->dimpl;
    for (int ch = 0; ch < 2; ++ch)
    {
        if ((in_buffers[ch] != NULL) && (out_buffers[ch] != NULL))
            distort(gc, in_buffers[ch], out_buffers[ch], buf_start, buf_stop);
    }

    return buf_stop;
}


