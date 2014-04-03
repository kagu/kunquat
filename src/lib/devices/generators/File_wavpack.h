

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010-2014
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#ifndef K_FILE_WAVPACK_H
#define K_FILE_WAVPACK_H


#include <stdbool.h>
#include <stdio.h>

#include <devices/generators/Sample.h>
#include <string/Streader.h>


bool Sample_parse_wavpack(Sample* sample, Streader* sr);


#endif // K_FILE_WAVPACK_H


