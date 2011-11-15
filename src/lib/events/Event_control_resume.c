

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010-2011
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

#include <Event_common.h>
#include <Event_control.h>
#include <Event_control_resume.h>
#include <General_state.h>
#include <xassert.h>


static Event_field_desc resume_desc[] =
{
    {
        .type = EVENT_FIELD_NONE
    }
};


Event_create_constructor(Event_control,
                         EVENT_CONTROL_RESUME,
                         resume);


bool Event_control_resume_process(General_state* gstate, char* fields)
{
    assert(gstate != NULL);
    (void)fields;
    if (!gstate->global)
    {
        return false;
    }
    gstate->pause = false;
    return true;
}


