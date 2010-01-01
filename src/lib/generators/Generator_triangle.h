

/*
 * Copyright 2010 Tomi Jylhä-Ollila
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


#ifndef K_GENERATOR_TRIANGLE_H
#define K_GENERATOR_TRIANGLE_H


#include <Generator.h>
#include <Voice_state.h>


typedef struct Generator_triangle
{
    Generator parent;
} Generator_triangle;


/**
 * Creates a new Triangle Generator.
 *
 * \param ins_params   The Instrument parameters -- must not be \c NULL.
 *
 * \return   The new Triangle Generator if successful, or \c NULL if memory
 *           allocation failed.
 */
Generator* new_Generator_triangle(Instrument_params* ins_params);


uint32_t Generator_triangle_mix(Generator* gen,
                                Voice_state* state,
                                uint32_t nframes,
                                uint32_t offset,
                                uint32_t freq,
                                double tempo,
                                int buf_count,
                                kqt_frame** bufs);


/**
 * Destroys an existing Triangle Generator.
 *
 * \param gen   The Triangle Generator -- must not be \c NULL.
 */
void del_Generator_triangle(Generator* gen);


#endif // K_GENERATOR_TRIANGLE_H


