

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
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

#include <Event_common.h>
#include <Event_channel_slide_force_length.h>
#include <Reltime.h>
#include <Voice.h>

#include <xmemory.h>


static Event_field_desc slide_force_length_desc[] =
{
    {
        .type = EVENT_FIELD_RELTIME,
        .min.field.Reltime_type = { 0, 0 },
        .max.field.Reltime_type = { INT64_MAX, KQT_RELTIME_BEAT - 1 }
    },
    {
        .type = EVENT_FIELD_NONE
    }
};


Event_create_set_reltime_and_get(Event_channel_slide_force_length,
                                 EVENT_CHANNEL_SLIDE_FORCE_LENGTH,
                                 length);


Event_create_constructor(Event_channel_slide_force_length,
                         EVENT_CHANNEL_SLIDE_FORCE_LENGTH,
                         slide_force_length_desc,
                         Reltime_set(&event->length, 0, 0));


bool Event_channel_slide_force_length_handle(Channel_state* ch_state, char* fields)
{
    assert(ch_state != NULL);
    if (fields == NULL)
    {
        return false;
    }
    Event_field data[1];
    Read_state* state = READ_STATE_AUTO;
    Event_type_get_fields(fields, slide_force_length_desc, data, state);
    if (state->error)
    {
        return false;
    }
    Reltime_copy(&ch_state->force_slide_length, &data[0].field.Reltime_type);
    uint32_t slide_frames = Reltime_toframes(&data[0].field.Reltime_type,
                                             *ch_state->tempo,
                                             *ch_state->freq);
    for (int i = 0; i < KQT_GENERATORS_MAX; ++i)
    {
        Event_check_voice(ch_state, i);
        Voice_state* vs = &ch_state->fg[i]->state.generic;
        Reltime_copy(&vs->force_slide_length, &data[0].field.Reltime_type);
        vs->force_slide_frames = slide_frames;
        if (vs->force_slide != 0)
        {
            double force_dB = log2(vs->force) * 6;
            double target_dB = log2(vs->force_slide_target) * 6;
            double dB_step = (target_dB - force_dB) / vs->force_slide_frames;
            vs->force_slide_update = exp2(dB_step / 6);
        }
    }
    return true;
}


