

/*
 * Author: Tomi Jylhä-Ollila, Finland 2015
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#ifndef K_ENVGEN_STATE_H
#define K_ENVGEN_STATE_H


#include <player/devices/Proc_state.h>
#include <player/devices/Voice_state.h>


size_t Envgen_vstate_get_size(void);

void Envgen_vstate_init(Voice_state* vstate, const Proc_state* proc_state);


#endif // K_ENVGEN_STATE_H


