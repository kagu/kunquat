

/*
 * Author: Tomi Jylhä-Ollila, Finland 2016
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#ifndef KQT_PROC_PANNING_H
#define KQT_PROC_PANNING_H


#include <init/devices/Device_impl.h>


typedef struct Proc_panning
{
    Device_impl parent;
    double panning;
} Proc_panning;


#endif // KQT_PROC_PANNING_H


