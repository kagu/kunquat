

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010-2019
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#ifndef KQT_EVENT_COMMON_H
#define KQT_EVENT_COMMON_H


#include <mathnum/Tstamp.h>

#include <stdbool.h>
#include <stdlib.h>


#define Event_get_voice_group(ch)                                           \
    ((ch)->fg_group_id != 0                                                 \
    ? Voice_pool_get_group((ch)->pool, (ch)->fg_group_id, VOICE_GROUP_AUTO) \
    : NULL)


#if 0
#define Event_check_voice(ch_state, proc)              \
    if (true)                                          \
    {                                                  \
        if ((ch_state)->fg[(proc)] == NULL)            \
            continue;                                  \
                                                       \
        (ch_state)->fg[(proc)] = Voice_pool_get_voice( \
                        (ch_state)->pool,              \
                        (ch_state)->fg[(proc)],        \
                        (ch_state)->fg_id[(proc)]);    \
        if ((ch_state)->fg[(proc)] == NULL)            \
            continue;                                  \
    }                                                  \
    else ignore(0)
#endif


#endif // KQT_EVENT_COMMON_H


