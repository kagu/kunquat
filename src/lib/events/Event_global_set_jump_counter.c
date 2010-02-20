

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
#include <limits.h>

#include <Event_common.h>
#include <Event_global_set_jump_counter.h>
#include <kunquat/limits.h>

#include <xmemory.h>


static Event_field_desc set_jump_counter_desc[] =
{
    {
        .type = EVENT_FIELD_INT,
        .min.field.integral_type = 0,
        .max.field.integral_type = 65535
    },
    {
        .type = EVENT_FIELD_NONE
    }
};


Event_create_set_primitive_and_get(Event_global_set_jump_counter,
                                   EVENT_GLOBAL_SET_JUMP_COUNTER,
                                   int64_t, counter);


Event_create_constructor(Event_global_set_jump_counter,
                         EVENT_GLOBAL_SET_JUMP_COUNTER,
                         set_jump_counter_desc,
                         event->counter = 0);


bool Event_global_set_jump_counter_handle(Playdata* global_state,
                                          char* fields)
{
    assert(global_state != NULL);
    if (fields == NULL)
    {
        return false;
    }
    Event_field data[1];
    Read_state* state = READ_STATE_AUTO;
    Event_type_get_fields(fields, set_jump_counter_desc, data, state);
    if (state->error)
    {
        return false;
    }
    global_state->jump_set_counter = data[0].field.integral_type;
    return true;
}


