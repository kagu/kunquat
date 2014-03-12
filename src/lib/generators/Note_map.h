

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


#ifndef K_NOTE_MAP_H
#define K_NOTE_MAP_H


#include <stdint.h>

#include <AAtree.h>
#include <Random.h>
#include <Sample_entry.h>
#include <Streader.h>


#define NOTE_MAP_RANDOMS_MAX (8)


typedef struct Note_map Note_map;


/**
 * Creates a new Note map from a string.
 *
 * \param sr   The Streader of the JSON data -- must not be \c NULL.
 *
 * \return   The new Note map if successful, otherwise \c NULL.
 */
Note_map* new_Note_map_from_string(Streader* sr);


/**
 * Adds a Sample entry into the Note map.
 *
 * This function is for Generators that create their own Note maps.
 *
 * \param map     The Note map -- must not be \c NULL.
 * \param cents   The pitch in cents -- must be finite.
 * \param force   The force in decibels -- must be finite.
 * \param entry   The Sample entry -- must not be \c NULL. The function
 *                ignores the ref_freq value in the supplied entry and
 *                calculates it from \a cents. The Note map does _not_
 *                assume ownership of \a entry so it can be automatically
 *                allocated.
 *
 * \return   \c true if successful, or \c false if memory allocation failed
 *           or this mapping position (\a cents, \a force) already contains
 *           \c NOTE_MAP_RANDOMS_MAX entries.
 */
bool Note_map_add_entry(
        Note_map* map, double cents, double force, Sample_entry* entry);


/**
 * Gets a Sample entry from the Note map.
 *
 * \param map      The Note map -- must not be \c NULL.
 * \param cents    The pitch in cents -- must be finite.
 * \param force    The force in dB -- must be finite or -INFINITY.
 * \param random   The Random source -- must not be \c NULL.
 *
 * \return   The closest Sample entry, or \c NULL if the map is empty.
 */
const Sample_entry* Note_map_get_entry(
        const Note_map* map,
        double cents,
        double force,
        Random* random);


/**
 * Destroys an existing Note map.
 *
 * \param map   The Note map, or \c NULL.
 */
void del_Note_map(Note_map* map);


#endif // K_NOTE_MAP_H

