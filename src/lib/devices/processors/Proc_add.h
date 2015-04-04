

/*
 * Author: Tomi Jylhä-Ollila, Finland 2011-2015
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#ifndef K_PROC_ADD_H
#define K_PROC_ADD_H


#include <stdint.h>

#include <devices/Device_impl.h>
#include <devices/Processor.h>


/**
 * Create a new additive synthesis Processor.
 *
 * \return   The new additive synthesis Processor if successful, or \c NULL if
 *           memory allocation failed.
 */
Device_impl* new_Proc_add(Processor* proc);


/**
 * Return additive Processor property information.
 *
 * \param proc            The additive Processor -- must be valid.
 * \param property_type   The property type -- must not be \c NULL.
 *
 * \return   The additive Processor property description matching
 *           \a property_type, or \c NULL if one does not exist.
 */
const char* Proc_add_property(const Processor* proc, const char* property_type);


#endif // K_PROC_ADD_H

