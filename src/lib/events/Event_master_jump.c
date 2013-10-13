

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010-2013
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
#include <stdio.h>
#include <limits.h>

#include <Event_common.h>
#include <Event_master_decl.h>
#include <kunquat/limits.h>
#include <xassert.h>


bool Event_master_jump_process(Master_params* master_params, Value* value)
{
    assert(master_params != NULL);
    (void)value;

    if (master_params->jump_counter <= 0)
        return true;

    // Get a new Jump context
    AAnode* handle = Jump_cache_acquire_context(master_params->jump_cache);
    if (handle == NULL)
    {
        fprintf(stderr, "Error: Out of jump contexts!\n");
        return false;
    }
    Jump_context* jc = AAnode_get_data(handle);

    // Init context
    jc->piref = master_params->cur_pos.piref;
    Tstamp_copy(&jc->row, &master_params->cur_pos.pat_pos);
    jc->ch_num = master_params->cur_ch;
    jc->order = master_params->cur_trigger;

    jc->counter = master_params->jump_counter;

    jc->target_piref = master_params->jump_target_piref;
    Tstamp_copy(&jc->target_row, &master_params->jump_target_row);

    // Store new context handle
    Active_jumps_add_context(master_params->active_jumps, handle);

    master_params->do_jump = true;

    return true;
}


