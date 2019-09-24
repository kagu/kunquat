

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


#ifndef KQT_PROC_KS_H
#define KQT_PROC_KS_H


#include <init/devices/Device_impl.h>


#define KS_MIN_DAMP 0
#define KS_MAX_DAMP 100
#define KS_DEFAULT_DAMP 100

#define KS_MIN_AUDIO_RATE_RANGE 1000
#define KS_MAX_AUDIO_RATE_RANGE 384000
#define KS_DEFAULT_AUDIO_RATE_RANGE_MIN 48000
#define KS_DEFAULT_AUDIO_RATE_RANGE_MAX 48000


typedef struct Proc_ks
{
    Device_impl parent;

    double damp;
    bool audio_rate_range_enabled;
    int32_t audio_rate_range_min;
    int32_t audio_rate_range_max;
} Proc_ks;


#endif // KQT_PROC_KS_H


