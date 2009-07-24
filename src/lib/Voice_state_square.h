

/*
 * Copyright 2009 Tomi Jylhä-Ollila
 *
 * This file is part of Kunquat.
 *
 * Kunquat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kunquat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kunquat.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef K_VOICE_STATE_SQUARE_H
#define K_VOICE_STATE_SQUARE_H


#include <Voice_state.h>


typedef struct Voice_state_square
{
    Voice_state parent;
    double phase;
    double pulse_width;
} Voice_state_square;


/**
 * Initialises the Square Instrument parameters.
 *
 * \param square   The Square parameters -- must not be \c NULL.
 */
void Voice_state_square_init(Voice_state* state);


#endif // K_VOICE_STATE_SQUARE_H

