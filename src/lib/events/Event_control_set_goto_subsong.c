

/*
 * Author: Tomi Jylhä-Ollila, Finland 2011
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <stdlib.h>
#include <stdbool.h>

#include <Event_control_set_goto_subsong.h>
#include <General_state.h>
#include <Playdata.h>
#include <xassert.h>


static Event_field_desc set_goto_subsong_desc[] =
{
    {
        .type = EVENT_FIELD_INT,
        .min.field.integral_type = -1,
        .max.field.integral_type = KQT_SUBSONGS_MAX - 1
    },
    {
        .type = EVENT_FIELD_NONE
    }
};


bool Event_control_set_goto_subsong_process(General_state* gstate,
                                            char* fields)
{
    assert(gstate != NULL);
    if (!gstate->global)
    {
        return false;
    }
    Playdata* global_state = (Playdata*)gstate;
    Event_field data[1];
    Read_state* state = READ_STATE_AUTO;
    Event_type_get_fields(fields, set_goto_subsong_desc, data, state);
    if (state->error)
    {
        return false;
    }
    global_state->goto_set_subsong = data[0].field.integral_type;
    return true;
}


