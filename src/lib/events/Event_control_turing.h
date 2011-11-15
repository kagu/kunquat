

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


#ifndef K_EVENT_CONTROL_TURING_H
#define K_EVENT_CONTROL_TURING_H


#include <stdbool.h>

#include <General_state.h>
#include <Reltime.h>


Event* new_Event_control_turing(Reltime* pos);


bool Event_control_turing_process(General_state* gstate, char* fields);


#endif // K_EVENT_CONTROL_TURING_H



