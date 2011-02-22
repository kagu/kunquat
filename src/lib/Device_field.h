

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010-2011
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#ifndef K_DEVICE_FIELD_H
#define K_DEVICE_FIELD_H


#include <stdbool.h>
#include <stdint.h>

#include <Envelope.h>
#include <File_base.h>
#include <Real.h>
#include <Reltime.h>
#include <Sample.h>
#include <Sample_params.h>
#include <Sample_map.h>


typedef enum
{
    DEVICE_FIELD_NONE = 0,
    DEVICE_FIELD_BOOL,
    DEVICE_FIELD_INT,
    DEVICE_FIELD_FLOAT,
    DEVICE_FIELD_REAL,
    DEVICE_FIELD_RELTIME,
    DEVICE_FIELD_ENVELOPE,
    DEVICE_FIELD_WAVPACK,
    DEVICE_FIELD_VORBIS,
    DEVICE_FIELD_SAMPLE_PARAMS,
    DEVICE_FIELD_SAMPLE_MAP,
} Device_field_type;


typedef struct Device_field Device_field;


/**
 * Creates a new Device field.
 *
 * \param key    The key -- must be a valid key. A valid Device field key
 *               has one of the following suffixes:
 *                  .jsonb (boolean)
 *                  .jsoni (int)
 *                  .jsonf (float)
 *                  .jsont (Reltime)
 *                  .jsone (Envelope)
 *                  .jsonsh (Sample params)
 *                  .jsonsm (Sample map)
 *                  .wv (WavPack).
 * \param data   Pointer to the data that must have a type matching the key,
 *               or \c NULL.
 *
 * \return   The new field if successful, or \c NULL if memory allocation
 *           failed.
 */
Device_field* new_Device_field(const char* key, void* data);


/**
 * Creates a new Device field from data.
 *
 * \param key      The key of the field -- must be a valid Device field key.
 * \param data     The data -- must not be \c NULL if it has a non-zero
 *                 length.
 * \param length   The length of the data -- must be >= \c 0.
 * \param state    The Read state -- must not be \c NULL.
 *
 * \return   The new Device field if successful, otherwise \c NULL.
 *           \a state will _not_ be modified if memory allocation failed.
 */
Device_field* new_Device_field_from_data(const char* key,
                                         void* data,
                                         long length,
                                         Read_state* state);


/**
 * Changes the data of a Device field.
 *
 * This is used for modifying _composition_ data (not playback data).
 *
 * \param field    The Device field -- must not be \c NULL.
 * \param data     The data -- must not be \c NULL if it has a non-zero
 *                 length.
 * \param length   The length of the data -- must be >= \c 0.
 * \param state    The Read state -- must not be \c NULL.
 *
 * \return   \c true if successful, otherwise \c false. \a state will _not_
 *           be modified if memory allocation failed.
 */
bool Device_field_change(Device_field* field,
                         void* data,
                         long length,
                         Read_state* state);


/**
 * Compares two Device fields.
 *
 * \param field1   The first field -- must not be \c NULL.
 * \param field2   The second field -- must not be \c NULL.
 *
 * \return   An integer less than, equal to or greater than zero if \a field1
 *           is found, respectively, to be less than, equal to or greater than
 *           \a field2.
 */
int Device_field_cmp(const Device_field* field1,
                     const Device_field* field2);


/**
 * Sets the empty flag of the Device field.
 *
 * \param field   The Device field -- must not be \c NULL. Fields that are
 *                not real-time are not supported.
 * \param flag    The empty flag.
 */
void Device_field_set_empty(Device_field* field, bool empty);


/**
 * Gets the empty flag of the Device field.
 *
 * \param field   The Device field -- must not be \c NULL.
 *
 * \return   The empty flag.
 */
bool Device_field_get_empty(Device_field* field);


/**
 * Modifies Device field data.
 *
 * \param field   The Device field -- must not be \c NULL and must not
 *                contain an Envelope or a Sample.
 * \param str     The new data as a string.
 *
 * \return   \c true if the Device field was successfully modified,
 *           otherwise \c false.
 */
bool Device_field_modify(Device_field* field, char* str);


/**
 * Gets a reference to a boolean value from the Device field.
 *
 * \param field   The Device field -- must not be \c NULL and must contain
 *                a boolean value.
 *
 * \return   The boolean value.
 */
bool* Device_field_get_bool(Device_field* field);


/**
 * Gets a reference to an integer value from the Device field.
 *
 * \param field   The Device field -- must not be \c NULL and must contain
 *                an integer value.
 *
 * \return   The integer value.
 */
int64_t* Device_field_get_int(Device_field* field);


/**
 * Gets a reference to a floating point value from the Device field.
 *
 * \param field   The Device field -- must not be \c NULL and must contain
 *                a floating point value.
 *
 * \return   The floating point value.
 */
double* Device_field_get_float(Device_field* field);


/**
 * Gets a Real value from the Device field.
 *
 * \param field   The Device field -- must not be \c NULL and must contain
 *                a Real value.
 *
 * \return   The Real value.
 */
Real* Device_field_get_real(Device_field* field);


/**
 * Gets a Reltime value from the Device field.
 *
 * \param field   The Device field -- must not be \c NULL and must contain
 *                a Reltime value.
 *
 * \return   The Reltime value.
 */
Reltime* Device_field_get_reltime(Device_field* field);


/**
 * Gets an Envelope value from the Device field.
 *
 * \param field   The Device field -- must not be \c NULL and must contain
 *                an Envelope value.
 *
 * \return   The Envelope value.
 */
Envelope* Device_field_get_envelope(Device_field* field);


/**
 * Gets a Sample from the Device field.
 *
 * \param field   The Device field -- must not be \c NULL and must contain
 *                a Sample.
 *
 * \return   The Sample.
 */
Sample* Device_field_get_sample(Device_field* field);


/**
 * Gets Sample parameters from the Device field.
 *
 * \param field   The Device field -- must not be \c NULL and must contain
 *                Sample parameters.
 *
 * \return   The Sample parameters.
 */
Sample_params* Device_field_get_sample_params(Device_field* field);


/**
 * Gets a Sample map from the Device field.
 *
 * \param field   The Device field -- must not be \c NULL and must contain
 *                a Sample.
 *
 * \return   The Sample map.
 */
Sample_map* Device_field_get_sample_map(Device_field* field);


/**
 * Destroys an existing Device field.
 *
 * \param field   The Device field, or \c NULL.
 */
void del_Device_field(Device_field* field);


#endif // K_DEVICE_FIELD_H


