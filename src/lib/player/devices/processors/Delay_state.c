

/*
 * Author: Tomi Jylhä-Ollila, Finland 2015-2019
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <player/devices/processors/Delay_state.h>

#include <init/devices/processors/Proc_delay.h>
#include <kunquat/limits.h>
#include <mathnum/common.h>
#include <mathnum/conversions.h>
#include <memory.h>
#include <player/devices/Device_thread_state.h>
#include <player/devices/processors/Proc_state_utils.h>
#include <player/Linear_controls.h>
#include <player/Player.h>
#include <player/Work_buffer.h>

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


typedef struct Delay_pstate
{
    Proc_state parent;

    Work_buffer* bufs[2];
    int32_t buf_pos;
} Delay_pstate;


static void del_Delay_pstate(Device_state* dstate)
{
    rassert(dstate != NULL);

    Delay_pstate* dpstate = (Delay_pstate*)dstate;

    for (int ch = 0; ch < 2; ++ch)
    {
        del_Work_buffer(dpstate->bufs[ch]);
        dpstate->bufs[ch] = NULL;
    }

    memory_free(dpstate);

    return;
}


static bool Delay_pstate_set_audio_rate(Device_state* dstate, int32_t audio_rate)
{
    rassert(dstate != NULL);
    rassert(audio_rate > 0);

    Delay_pstate* dpstate = (Delay_pstate*)dstate;

    const Proc_delay* delay = (const Proc_delay*)dstate->device->dimpl;

    const int32_t delay_buf_size = (int32_t)(delay->max_delay * audio_rate + 1);

    for (int ch = 0; ch < 2; ++ch)
    {
        if (!Work_buffer_resize(dpstate->bufs[ch], delay_buf_size))
            return false;
        Work_buffer_clear(dpstate->bufs[ch], 0, Work_buffer_get_size(dpstate->bufs[ch]));
    }

    dpstate->buf_pos = 0;

    return true;
}


static void Delay_pstate_reset(Device_state* dstate)
{
    rassert(dstate != NULL);

    Delay_pstate* dpstate = (Delay_pstate*)dstate;

    for (int ch = 0; ch < 2; ++ch)
        Work_buffer_clear(dpstate->bufs[ch], 0, Work_buffer_get_size(dpstate->bufs[ch]));

    dpstate->buf_pos = 0;

    return;
}


enum
{
    PORT_IN_AUDIO_L = 0,
    PORT_IN_AUDIO_R,
    PORT_IN_DELAY,
    PORT_IN_COUNT,

    PORT_IN_AUDIO_COUNT = PORT_IN_AUDIO_R + 1
};

enum
{
    PORT_OUT_AUDIO_L = 0,
    PORT_OUT_AUDIO_R,
    PORT_OUT_COUNT
};


static const int DELAY_WB_FIXED_INPUT = WORK_BUFFER_IMPL_1;
static const int DELAY_WB_TOTAL_OFFSETS = WORK_BUFFER_IMPL_2;
static const int DELAY_WB_FIXED_DELAY = WORK_BUFFER_IMPL_3;


static void Delay_pstate_render_mixed(
        Device_state* dstate,
        Device_thread_state* proc_ts,
        const Work_buffers* wbs,
        int32_t frame_count,
        double tempo)
{
    rassert(dstate != NULL);
    rassert(proc_ts != NULL);
    rassert(wbs != NULL);
    rassert(frame_count > 0);
    rassert(tempo > 0);

    Delay_pstate* dpstate = (Delay_pstate*)dstate;

    const Proc_delay* delay = (const Proc_delay*)dstate->device->dimpl;

    float* in_bufs[2] = { NULL };
    for (int ch = 0; ch < 2; ++ch)
    {
        Work_buffer* in_wb = Device_thread_state_get_mixed_buffer(
                proc_ts, DEVICE_PORT_TYPE_RECV, PORT_IN_AUDIO_L + ch);
        if (!Work_buffer_is_valid(in_wb))
        {
            in_wb = Work_buffers_get_buffer_mut(wbs, DELAY_WB_FIXED_INPUT);
            Work_buffer_clear(in_wb, 0, frame_count);
        }

        in_bufs[ch] = Work_buffer_get_contents_mut(in_wb);
    }

    float* out_bufs[2] = { NULL };
    for (int ch = 0; ch < 2; ++ch)
    {
        Work_buffer* out_wb = Device_thread_state_get_mixed_buffer(
                proc_ts, DEVICE_PORT_TYPE_SEND, PORT_OUT_AUDIO_L + ch);
        if (out_wb != NULL)
            out_bufs[ch] = Work_buffer_get_contents_mut(out_wb);
    }

    float* history_data[2] = { NULL };
    for (int ch = 0; ch < 2; ++ch)
        history_data[ch] = Work_buffer_get_contents_mut(dpstate->bufs[ch]);

    const int32_t delay_buf_size = Work_buffer_get_size(dpstate->bufs[0]);
    const int32_t delay_max = delay_buf_size - 1;

    float* total_offsets =
        Work_buffers_get_buffer_contents_mut(wbs, DELAY_WB_TOTAL_OFFSETS);

    // Get delay stream
    Work_buffer* delays_wb = Device_thread_state_get_mixed_buffer(
            proc_ts, DEVICE_PORT_TYPE_RECV, PORT_IN_DELAY);
    float* delays = Work_buffer_is_valid(delays_wb)
            ? Work_buffer_get_contents_mut(delays_wb) : NULL;
    if (delays == NULL)
    {
        delays = Work_buffers_get_buffer_contents_mut(wbs, DELAY_WB_FIXED_DELAY);
        const float init_delay = (float)delay->init_delay;
        for (int32_t i = 0; i < frame_count; ++i)
            delays[i] = init_delay;
    }

    int32_t cur_dpstate_buf_pos = dpstate->buf_pos;

    const int32_t audio_rate = dstate->audio_rate;

    // Get total offsets
    for (int32_t i = 0, chunk_offset = 0; i < frame_count; ++i, ++chunk_offset)
    {
        const float cur_delay = delays[i];
        double delay_frames = cur_delay * (double)audio_rate;
        delay_frames = clamp(delay_frames, 0, delay_max);
        total_offsets[i] = (float)(chunk_offset - delay_frames);
    }

    // Clamp input values to finite range
    // (this is required due to possible multiplication by zero below)
    for (int ch = 0; ch < 2; ++ch)
    {
        float* in_buf = in_bufs[ch];
        if (in_buf == NULL)
            continue;

        for (int32_t i = 0; i < frame_count; ++i)
            in_buf[i] = clamp(in_buf[i], -FLT_MAX, FLT_MAX);
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        const float* in_buf = in_bufs[ch];
        float* out_buf = out_bufs[ch];
        if ((in_buf == NULL) || (out_buf == NULL))
            continue;

        const float* history = history_data[ch];

        for (int32_t i = 0; i < frame_count; ++i)
        {
            const float total_offset = total_offsets[i];

            // Get buffer positions
            const int32_t cur_pos = (int32_t)floor(total_offset);
            const float remainder = total_offset - (float)cur_pos;
            rassert(cur_pos <= (int32_t)i);
            rassert(implies(cur_pos == (int32_t)i, remainder == 0));
            const int32_t next_pos = cur_pos + 1;

            // Get audio frames
            float cur_val = 0;
            float next_val = 0;

            if (cur_pos >= 0)
            {
                rassert(cur_pos < frame_count);
                cur_val = in_buf[cur_pos];

                const int32_t in_next_pos = min(next_pos, i);
                next_val = in_buf[in_next_pos];
            }
            else
            {
                const int32_t cur_delay_buf_pos =
                    (cur_dpstate_buf_pos + cur_pos + delay_buf_size) % delay_buf_size;
                rassert(cur_delay_buf_pos >= 0);

                cur_val = history[cur_delay_buf_pos];

                if (next_pos < 0)
                {
                    const int32_t next_delay_buf_pos =
                        (cur_dpstate_buf_pos + next_pos + delay_buf_size) % delay_buf_size;
                    rassert(next_delay_buf_pos >= 0);

                    next_val = history[next_delay_buf_pos];
                }
                else
                {
                    rassert(next_pos == 0);
                    next_val = in_buf[0];
                }
            }

            // Create output frame
            out_buf[i] = lerp(cur_val, next_val, remainder);
        }
    }

    const int32_t init_dpstate_buf_pos = cur_dpstate_buf_pos;

    for (int ch = 0; ch < 2; ++ch)
    {
        const float* in_buf = in_bufs[ch];
        if (in_buf == NULL)
            continue;

        float* history = history_data[ch];

        cur_dpstate_buf_pos = init_dpstate_buf_pos;

        for (int32_t i = 0; i < frame_count; ++i)
        {
            history[cur_dpstate_buf_pos] = in_buf[i];

            ++cur_dpstate_buf_pos;
            if (cur_dpstate_buf_pos >= delay_buf_size)
            {
                rassert(cur_dpstate_buf_pos == delay_buf_size);
                cur_dpstate_buf_pos = 0;
            }
        }
    }

    dpstate->buf_pos = cur_dpstate_buf_pos;

    return;
}


static void Delay_pstate_clear_history(Proc_state* proc_state)
{
    rassert(proc_state != NULL);

    Delay_pstate* dpstate = (Delay_pstate*)proc_state;

    for (int ch = 0; ch < 2; ++ch)
        Work_buffer_clear(dpstate->bufs[ch], 0, Work_buffer_get_size(dpstate->bufs[ch]));

    dpstate->buf_pos = 0;

    return;
}


Device_state* new_Delay_pstate(
        const Device* device, int32_t audio_rate, int32_t audio_buffer_size)
{
    rassert(device != NULL);
    rassert(audio_rate > 0);
    rassert(audio_buffer_size >= 0);

    Delay_pstate* dpstate = memory_alloc_item(Delay_pstate);
    if (dpstate == NULL)
        return NULL;

    if (!Proc_state_init(&dpstate->parent, device, audio_rate, audio_buffer_size))
    {
        memory_free(dpstate);
        return NULL;
    }

    dpstate->parent.destroy = del_Delay_pstate;
    dpstate->parent.set_audio_rate = Delay_pstate_set_audio_rate;
    dpstate->parent.reset = Delay_pstate_reset;
    dpstate->parent.render_mixed = Delay_pstate_render_mixed;
    dpstate->parent.clear_history = Delay_pstate_clear_history;
    dpstate->buf_pos = 0;

    for (int ch = 0; ch < 2; ++ch)
        dpstate->bufs[ch] = NULL;

    const Proc_delay* delay = (const Proc_delay*)device->dimpl;

    const int32_t delay_buf_size = (int32_t)(delay->max_delay * audio_rate + 1);

    for (int ch = 0; ch < 2; ++ch)
    {
        dpstate->bufs[ch] = new_Work_buffer(delay_buf_size);
        if (dpstate->bufs[ch] == NULL)
        {
            del_Device_state(&dpstate->parent.parent);
            return NULL;
        }
    }

    return &dpstate->parent.parent;
}


bool Delay_pstate_set_max_delay(
        Device_state* dstate, const Key_indices indices, double value)
{
    rassert(dstate != NULL);
    ignore(indices);
    ignore(value);

    Delay_pstate* dpstate = (Delay_pstate*)dstate;

    const Proc_delay* delay = (const Proc_delay*)dstate->device->dimpl;

    const int32_t delay_buf_size = (int32_t)(delay->max_delay * dstate->audio_rate + 1);

    for (int ch = 0; ch < 2; ++ch)
    {
        if (!Work_buffer_resize(dpstate->bufs[ch], delay_buf_size))
            return false;
        Work_buffer_clear(dpstate->bufs[ch], 0, Work_buffer_get_size(dpstate->bufs[ch]));
    }

    dpstate->buf_pos = 0;

    return true;
}


