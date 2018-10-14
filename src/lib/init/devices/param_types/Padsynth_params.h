

/*
 * Author: Tomi Jylhä-Ollila, Finland 2016-2018
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#ifndef KQT_PADSYNTH_PARAMS_H
#define KQT_PADSYNTH_PARAMS_H


#include <decl.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


#define PADSYNTH_MAX_SAMPLE_COUNT 128

#define PADSYNTH_MIN_SAMPLE_LENGTH 16384
#define PADSYNTH_DEFAULT_SAMPLE_LENGTH 262144
#define PADSYNTH_MAX_SAMPLE_LENGTH 1048576

#define PADSYNTH_DEFAULT_AUDIO_RATE 48000

#define PADSYNTH_DEFAULT_BANDWIDTH_BASE 1
#define PADSYNTH_DEFAULT_BANDWIDTH_SCALE 1


typedef struct Padsynth_harmonic
{
    double freq_mul;
    double amplitude;
} Padsynth_harmonic;


typedef struct Padsynth_params
{
    int32_t sample_length;
    int32_t audio_rate;
    int sample_count;
    double min_pitch;
    double max_pitch;
    double centre_pitch;

    double bandwidth_base;
    double bandwidth_scale;
    Vector* harmonics;

    bool is_res_env_enabled;
    Envelope* res_env;
} Padsynth_params;


/**
 * Create new PADsynth parameters.
 *
 * \param sr   The Streader of the JSON input -- must not be \c NULL.
 *
 * \return   The new PADsynth parameters if successful, otherwise \c NULL.
 */
Padsynth_params* new_Padsynth_params(Streader* sr);


/**
 * Deinitialise PADsynth parameters.
 *
 * \param pp   The PADsynth parameters, or \c NULL.
 */
void del_Padsynth_params(Padsynth_params* pp);


#endif // KQT_PADSYNTH_PARAMS_H


