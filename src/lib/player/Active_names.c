

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


#include <player/Active_names.h>

#include <debug/assert.h>
#include <init/Env_var.h>
#include <kunquat/limits.h>
#include <memory.h>

#include <stdlib.h>
#include <string.h>


struct Active_names
{
    char names[ACTIVE_CAT_COUNT][KQT_KEY_LENGTH_MAX];
};


Active_names* new_Active_names(void)
{
    Active_names* names = memory_alloc_item(Active_names);
    if (names == NULL)
        return NULL;

    Active_names_reset(names);
    return names;
}


bool Active_names_set(Active_names* names, Active_cat cat, const char* name)
{
    assert(names != NULL);
    assert(cat < ACTIVE_CAT_COUNT);
    assert(name != NULL);

    const size_t length_limit =
        (cat == ACTIVE_CAT_ENV) ? KQT_VAR_NAME_MAX : KQT_KEY_LENGTH_MAX;
    if (strlen(name) >= length_limit)
        return false;

    strcpy(names->names[cat], name);

    return true;
}


const char* Active_names_get(const Active_names* names, Active_cat cat)
{
    assert(names != NULL);
    assert(cat < ACTIVE_CAT_COUNT);

    return names->names[cat];
}


void Active_names_reset(Active_names* names)
{
    assert(names != NULL);
    memset(names->names, '\0', sizeof(names->names));
    return;
}


void del_Active_names(Active_names* names)
{
    if (names == NULL)
        return;

    memory_free(names);
    return;
}


