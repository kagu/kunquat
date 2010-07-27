

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


#ifndef K_DSP_TYPE_H
#define K_DSP_TYPE_H


#include <stdint.h>

#include <DSP.h>


/**
 * This is the type of a DSP constructor.
 *
 * \param buffer_size   The mixing buffer size -- must be > \c 0 and
 *                      <= \c KQT_BUFFER_SIZE_MAX.
 * \param mix_rate      The mixing rate -- must be > \c 0.
 *
 * \return   The new DSP if successful, or \c NULL if memory allocation
 *           failed.
 */
typedef DSP* DSP_cons(uint32_t buffer_size, uint32_t mix_rate);


/**
 * This is the type of a DSP property function.
 *
 * A DSP property function is used to retrieve information about the internal
 * operation of the DSP. The property is returned as JSON data.
 *
 * \param dsp             The DSP -- must not be \c NULL.
 * \param property_type   The property to resolve -- must not be \c NULL.
 *
 * \return   The property for the DSP if one exists, otherwise \c NULL.
 */
typedef char* DSP_property(DSP* dsp, const char* property_type);


typedef struct DSP_type DSP_type;


/**
 * Finds a DSP constructor.
 *
 * \param type   The DSP type -- must not be \c NULL.
 *
 * \return   The constructor if \a type is supported, otherwise \c NULL.
 */
DSP_cons* DSP_type_find_cons(char* type);


/**
 * Finds a DSP property function.
 *
 * \param type   The DSP type -- must be a valid type.
 *
 * \return   The property function if one exists, otherwise \c NULL.
 */
DSP_property* DSP_type_find_property(char* type);


#endif // K_DSP_TYPE_H

