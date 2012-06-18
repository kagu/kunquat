

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include <Generator_common.h>
#include <Generator_debug.h>
#include <Device_params.h>
#include <string_common.h>
#include <xassert.h>
#include <xmemory.h>


Generator* new_Generator_debug(uint32_t buffer_size,
                               uint32_t mix_rate)
{
    assert(buffer_size > 0);
    assert(buffer_size <= KQT_BUFFER_SIZE_MAX);
    assert(mix_rate > 0);
    Generator_debug* debug = xalloc(Generator_debug);
    if (debug == NULL)
    {
        return NULL;
    }
    if (!Generator_init(&debug->parent,
                        del_Generator_debug,
                        Generator_debug_mix,
                        NULL,
                        buffer_size,
                        mix_rate))
    {
        xfree(debug);
        return NULL;
    }
    return &debug->parent;
}


uint32_t Generator_debug_mix(Generator* gen,
                             Voice_state* state,
                             uint32_t nframes,
                             uint32_t offset,
                             uint32_t freq,
                             double tempo)
{
    assert(gen != NULL);
    assert(string_eq(gen->type, "debug"));
    assert(state != NULL);
    assert(freq > 0);
    assert(tempo > 0);
    (void)tempo;
    kqt_frame* bufs[] = { NULL, NULL };
    Generator_common_get_buffers(gen, state, offset, bufs);
    if (!state->active)
    {
        return offset;
    }
    for (uint32_t i = offset; i < nframes; ++i)
    {
        double val_l = 0;
        double val_r = 0;
        if (state->rel_pos == 0)
        {
            val_l = 1.0;
            val_r = 1.0;
            state->rel_pos = 1;
        }
        else
        {
            val_l = 0.5;
            val_r = 0.5;
        }
        if (!state->note_on)
        {
            val_l = -val_l;
            val_r = -val_r;
        }
        bufs[0][i] += val_l;
        bufs[1][i] += val_r;
        state->rel_pos_rem += state->pitch / freq;
        if (!state->note_on)
        {
            state->noff_pos_rem += state->pitch / freq;
            if (state->noff_pos_rem >= 2)
            {
                state->active = false;
                return i;
            }
        }
        if (state->rel_pos_rem >= 1)
        {
            ++state->pos;
            if (state->pos >= 10)
            {
                state->active = false;
                return i;
            }
            state->rel_pos = 0;
            state->rel_pos_rem -= floor(state->rel_pos_rem);
        }
    }
    return nframes;
}


void del_Generator_debug(Generator* gen)
{
    if (gen == NULL)
    {
        return;
    }
    assert(string_eq(gen->type, "debug"));
    Generator_debug* debug = (Generator_debug*)gen;
    xfree(debug);
    return;
}


