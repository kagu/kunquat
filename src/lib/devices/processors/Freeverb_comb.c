

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


#include <devices/processors/Freeverb_comb.h>

#include <debug/assert.h>
#include <mathnum/common.h>
#include <memory.h>

#include <stdint.h>
#include <stdlib.h>


struct Freeverb_comb
{
    float feedback;
    float damp1;
    float damp2;

    float filter_store;
    float* buffer;
    uint32_t buffer_size;
    uint32_t buffer_pos;
};


Freeverb_comb* new_Freeverb_comb(uint32_t buffer_size)
{
    assert(buffer_size > 0);

    Freeverb_comb* comb = memory_alloc_item(Freeverb_comb);
    if (comb == NULL)
        return NULL;

    comb->feedback = 0;
    comb->damp1 = 0;
    comb->damp2 = 0;

    comb->filter_store = 0;
    comb->buffer = NULL;
    comb->buffer_size = 0;
    comb->buffer_pos = 0;

    comb->buffer = memory_alloc_items(float, buffer_size);
    if (comb->buffer == NULL)
    {
        del_Freeverb_comb(comb);
        return NULL;
    }
    comb->buffer_size = buffer_size;
    Freeverb_comb_clear(comb);

    return comb;
}


void Freeverb_comb_set_damp(Freeverb_comb* comb, float damp)
{
    assert(comb != NULL);
    assert(damp >= 0);
    assert(damp <= 1);

    comb->damp1 = damp;
    comb->damp2 = 1 - damp;

    return;
}


void Freeverb_comb_set_feedback(Freeverb_comb* comb, float feedback)
{
    assert(comb != NULL);
    assert(feedback >= 0);
    assert(feedback < 1);

    comb->feedback = feedback;

    return;
}


float Freeverb_comb_process(Freeverb_comb* comb, float input)
{
    assert(comb != NULL);

    float output = comb->buffer[comb->buffer_pos];
    output = undenormalise(output);
    comb->filter_store = (output * comb->damp2) +
                         (comb->filter_store * comb->damp1);
    comb->filter_store = undenormalise(comb->filter_store);
    comb->buffer[comb->buffer_pos] = input + (comb->filter_store * comb->feedback);

    ++comb->buffer_pos;
    if (comb->buffer_pos >= comb->buffer_size)
        comb->buffer_pos = 0;

    return output;
}


bool Freeverb_comb_resize_buffer(Freeverb_comb* comb, uint32_t new_size)
{
    assert(comb != NULL);
    assert(new_size > 0);

    if (new_size == comb->buffer_size)
        return true;

    float* buffer = memory_realloc_items(float, new_size, comb->buffer);
    if (buffer == NULL)
        return false;

    comb->buffer = buffer;
    comb->buffer_size = new_size;
    Freeverb_comb_clear(comb);
    comb->buffer_pos = 0;

    return true;
}


void Freeverb_comb_clear(Freeverb_comb* comb)
{
    assert(comb != NULL);
    assert(comb->buffer != NULL);

    comb->filter_store = 0;

    for (uint32_t i = 0; i < comb->buffer_size; ++i)
        comb->buffer[i] = 0;

    return;
}


void del_Freeverb_comb(Freeverb_comb* comb)
{
    if (comb == NULL)
        return;

    memory_free(comb->buffer);
    memory_free(comb);

    return;
}


