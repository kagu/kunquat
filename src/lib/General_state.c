

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

#include <General_state.h>
#include <xassert.h>


General_state* General_state_init(General_state* state, bool global)
{
    assert(state != NULL);
    state->global = global;
    state->pause = false;
    return state;
}

