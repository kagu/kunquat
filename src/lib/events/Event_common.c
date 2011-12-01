

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


#include <stdlib.h>

#include <Event_common.h>
#include <xassert.h>
#include <xmemory.h>


Event* Event_init(Event* event,
                  Reltime* pos,
                  Event_type type,
                  Event_field_desc* field_types)
{
    assert(event != NULL);
    assert(pos != NULL);
    assert(EVENT_IS_VALID(type));
    assert(field_types != NULL);
    event->type = type;
    Reltime_copy(&event->pos, pos);
    event->field_types = field_types;
    event->desc = NULL;
    event->fields = NULL;
    event->destroy = del_Event_default;
    return event;
}


void del_Event_default(Event* event)
{
    if (event == NULL)
    {
        return;
    }
    assert(EVENT_IS_VALID(event->type));
    xfree(event->desc);
    xfree(event);
    return;
}


