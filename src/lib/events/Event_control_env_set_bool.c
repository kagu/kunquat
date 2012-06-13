

/*
 * Author: Tomi Jylhä-Ollila, Finland 2011-2012
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

#include <Active_names.h>
#include <Env_var.h>
#include <Environment.h>
#include <Event.h>
#include <Event_common.h>
#include <Event_control_env_set_bool.h>
#include <Event_type.h>
#include <File_base.h>
#include <General_state.h>
#include <Value.h>
#include <xassert.h>


bool Event_control_env_set_bool_process(General_state* gstate, Value* value)
{
    assert(gstate != NULL);
    assert(value != NULL);
    if (value->type != VALUE_TYPE_BOOL || !gstate->global)
    {
        return false;
    }
    Env_var* var = Environment_get(gstate->env,
                        Active_names_get(gstate->active_names,
                                         ACTIVE_CAT_ENV,
                                         ACTIVE_TYPE_BOOL));
    if (var == NULL || Env_var_get_type(var) != ENV_VAR_BOOL)
    {
        return true;
    }
    Env_var_modify_value(var, &value->value.bool_type);
    return true;
}

