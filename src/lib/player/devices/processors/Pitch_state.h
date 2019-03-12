

/*
 * Author: Tomi Jylhä-Ollila, Finland 2016-2019
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#ifndef KQT_PITCH_STATE_H
#define KQT_PITCH_STATE_H


#include <decl.h>
#include <player/devices/Proc_state.h>
#include <player/devices/Voice_state.h>
#include <player/Pitch_controls.h>


Voice_state_get_size_func Pitch_vstate_get_size;
Voice_state_init_func Pitch_vstate_init;
Voice_state_render_voice_func Pitch_vstate_render_voice;

void Pitch_vstate_set_controls(Voice_state* vstate, const Pitch_controls* controls);


#endif // KQT_PITCH_STATE_H


