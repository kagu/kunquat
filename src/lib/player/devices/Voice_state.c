

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010-2018
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <player/devices/Voice_state.h>

#include <debug/assert.h>
#include <init/devices/Au_expressions.h>
#include <init/devices/Audio_unit.h>
#include <init/devices/Device_impl.h>
#include <init/devices/Param_proc_filter.h>
#include <init/devices/Proc_type.h>
#include <init/devices/Processor.h>
#include <kunquat/limits.h>
#include <mathnum/Tstamp.h>
#include <player/Device_states.h>
#include <player/devices/Device_thread_state.h>
#include <player/Slider.h>

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


Voice_state* Voice_state_init(
        Voice_state* state, Proc_type proc_type, Random* rand_p, Random* rand_s)
{
    rassert(state != NULL);
    rassert(proc_type >= 0);
    rassert(proc_type < Proc_type_COUNT);
    rassert(rand_p != NULL);
    rassert(rand_s != NULL);

    Voice_state_clear(state);

    state->proc_type = proc_type;

    state->active = true;
    state->keep_alive_stop = 0;
    state->note_on = true;
    state->rand_p = rand_p;
    state->rand_s = rand_s;
    state->wb = NULL;

    return state;
}


void Voice_state_set_work_buffer(Voice_state* state, Work_buffer* wb)
{
    rassert(state != NULL);
    state->wb = wb;
    return;
}


Voice_state* Voice_state_clear(Voice_state* state)
{
    rassert(state != NULL);

    state->proc_type = Proc_type_COUNT;

    state->active = false;
    state->keep_alive_stop = 0;
    state->ramp_attack = 0;

    state->expr_filters_applied = false;
    memset(state->ch_expr_name, '\0', KQT_VAR_NAME_MAX + 1);
    memset(state->note_expr_name, '\0', KQT_VAR_NAME_MAX + 1);

    memset(state->test_proc_param, '\0', KQT_VAR_NAME_MAX + 1);

    state->hit_index = -1;

    state->pos = 0;
    state->pos_rem = 0;
    state->rel_pos = 0;
    state->rel_pos_rem = 0;
    state->dir = 1;
    state->note_on = false;
    state->noff_pos = 0;
    state->noff_pos_rem = 0;

    return state;
}


static bool is_proc_filtered(
        const Processor* proc, const Au_expressions* ae, const char* expr_name)
{
    rassert(proc != NULL);
    rassert(ae != NULL);
    rassert(expr_name != NULL);

    if (expr_name[0] == '\0')
        return false;

    const Param_proc_filter* proc_filter = Au_expressions_get_proc_filter(ae, expr_name);
    if (proc_filter == NULL)
        return false;

    return !Param_proc_filter_is_proc_allowed(proc_filter, proc->index);
}


int32_t Voice_state_render_voice(
        Voice_state* vstate,
        Proc_state* proc_state,
        const Device_thread_state* proc_ts,
        const Au_state* au_state,
        const Work_buffers* wbs,
        int32_t frame_count,
        double tempo)
{
    rassert(proc_state != NULL);
    rassert(proc_ts != NULL);
    rassert(au_state != NULL);
    rassert(wbs != NULL);
    rassert(frame_count >= 0);
    rassert(isfinite(tempo));
    rassert(tempo > 0);

    const Device* device = proc_state->parent.device;
    rassert(device != NULL);

    const Device_impl* dimpl = device->dimpl;
    rassert(dimpl != NULL);

    const int32_t vstate_size = (dimpl->get_vstate_size != NULL)
        ? dimpl->get_vstate_size() : (int32_t)sizeof(Voice_state);
    rassert((vstate == NULL) == (vstate_size == 0));

    const Processor* proc = (const Processor*)device;
    if (!Processor_get_voice_signals(proc) || (dimpl->render_voice == NULL))
    {
        if (vstate != NULL)
            vstate->active = false;
        return 0;
    }

    if ((vstate != NULL) && !vstate->expr_filters_applied)
    {
        // Stop processing if we are filtered out by current Audio unit expressions
        const Audio_unit* au = (const Audio_unit*)au_state->parent.device;
        const Au_expressions* ae = Audio_unit_get_expressions(au);
        if (ae != NULL)
        {
            if (is_proc_filtered(proc, ae, vstate->ch_expr_name) ||
                    is_proc_filtered(proc, ae, vstate->note_expr_name))
            {
                vstate->active = false;
                return 0;
            }
        }

        vstate->expr_filters_applied = true;
    }

    if (frame_count == 0)
        return 0;

    // Call the implementation
    const int32_t impl_rendered_count = dimpl->render_voice(
            vstate, proc_state, proc_ts, au_state, wbs, frame_count, tempo);
    rassert(impl_rendered_count <= frame_count);

    return impl_rendered_count;
}


void Voice_state_set_keep_alive_stop(Voice_state* vstate, int32_t stop)
{
    rassert(vstate != NULL);
    rassert(stop >= 0);

    vstate->keep_alive_stop = stop;

    return;
}


