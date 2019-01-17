

/*
 * Author: Tomi Jylhä-Ollila, Finland 2012-2019
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <init/Bind.h>

#include <containers/AAtree.h>
#include <debug/assert.h>
#include <expr.h>
#include <kunquat/limits.h>
#include <memory.h>
#include <player/Event_cache.h>
#include <player/Event_names.h>
#include <player/Event_type.h>
#include <Value.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct Bind
{
    AAtree* cblists;
};


typedef struct Constraint
{
    char event_name[KQT_EVENT_NAME_MAX + 1];
    char* expr;
    struct Constraint* next;
} Constraint;


static Constraint* new_Constraint(Streader* sr);


static bool Constraint_match(
        Constraint* constraint, Event_cache* cache, Env_state* estate, Random* rand);


static void del_Constraint(Constraint* constraint);


static Target_event* new_Target_event(Streader* sr, const Event_names* names);


static void del_Target_event(Target_event* event);


typedef struct Cblist_item
{
    Constraint* constraints;
    Target_event* first_event;
    Target_event* last_event;
    struct Cblist_item* next;
} Cblist_item;


static Cblist_item* new_Cblist_item(void);


static void del_Cblist_item(Cblist_item* item);


typedef enum
{
    SOURCE_STATE_NEW = 0,
    SOURCE_STATE_REACHED,
    SOURCE_STATE_VISITED
} Source_state;


typedef struct Cblist
{
    //char event_name[KQT_EVENT_NAME_MAX + 1];
    Event_type event_type;
    Source_state source_state;
    Cblist_item* first;
    Cblist_item* last;
} Cblist;


#define CBLIST_KEY(type) (&(Cblist){        \
        .event_type = (type),               \
        .source_state = SOURCE_STATE_NEW,   \
        .first = NULL,                      \
        .last = NULL })


static Cblist* new_Cblist(Event_type event_type);


static int Cblist_cmp(const Cblist* c1, const Cblist* c2);


static void Cblist_append(Cblist* list, Cblist_item* item);


static void del_Cblist(Cblist* list);


static bool read_constraints(Streader* sr, Bind* map, Cblist_item* item);


static bool read_events(Streader* sr, Cblist_item* item, const Event_names* names);


static bool Bind_is_cyclic(const Bind* map, const Event_names* event_names);


typedef struct bedata
{
    Bind* map;
    const Event_names* names;
} bedata;

static bool read_bind_entry(Streader* sr, int32_t index, void* userdata)
{
    rassert(sr != NULL);
    rassert(userdata != NULL);
    ignore(index);

    bedata* bd = userdata;

    char event_name[KQT_EVENT_NAME_MAX + 1] = "";
    if (!Streader_readf(sr, "[%s,", READF_STR(KQT_EVENT_NAME_MAX + 1, event_name)))
        return false;

    Event_type event_type = Event_names_get(bd->names, event_name);
    if (event_type == Event_NONE)
    {
        Streader_set_error(sr, "Event is not valid: %s", event_name);
        return false;
    }

    Cblist* cblist = AAtree_get_exact(bd->map->cblists, CBLIST_KEY(event_type));
    if (cblist == NULL)
    {
        cblist = new_Cblist(event_type);
        if (cblist == NULL || !AAtree_ins(bd->map->cblists, cblist))
        {
            del_Cblist(cblist);
            Streader_set_memory_error(sr, "Could not allocate memory for bind");
            return false;
        }
    }

    Cblist_item* item = new_Cblist_item();
    if (item == NULL)
    {
        Streader_set_memory_error(sr, "Could not allocate memory for bind");
        return false;
    }
    Cblist_append(cblist, item);

    if (!(read_constraints(sr, bd->map, item) &&
                Streader_match_char(sr, ',') &&
                read_events(sr, item, bd->names))
       )
        return false;

    return Streader_match_char(sr, ']');
}

Bind* new_Bind(Streader* sr, const Event_names* names)
{
    rassert(sr != NULL);
    rassert(names != NULL);

    if (Streader_is_error_set(sr))
        return NULL;

    Bind* map = memory_alloc_item(Bind);
    if (map == NULL)
    {
        Streader_set_memory_error(sr, "Could not allocate memory for bind");
        return NULL;
    }

    map->cblists = new_AAtree(
            (AAtree_item_cmp*)Cblist_cmp, (AAtree_item_destroy*)del_Cblist);
    if (map->cblists == NULL)
    {
        del_Bind(map);
        Streader_set_memory_error(sr, "Could not allocate memory for bind");
        return NULL;
    }

    if (!Streader_has_data(sr))
        return map;

    bedata* bd = &(bedata){ .map = map, .names = names, };

    if (!Streader_read_list(sr, read_bind_entry, bd))
    {
        del_Bind(map);
        return NULL;
    }

    if (Bind_is_cyclic(map, names))
    {
        Streader_set_error(sr, "Bind contains a cycle");
        del_Bind(map);
        return NULL;
    }

    return map;
}


Event_cache* Bind_create_cache(const Bind* map)
{
    rassert(map != NULL);

    Event_cache* cache = new_Event_cache();
    if (cache == NULL)
        return NULL;

    AAiter* iter = AAiter_init(AAITER_AUTO, map->cblists);
    Cblist* cblist = AAiter_get_at_least(iter, CBLIST_KEY(Event_NONE));
    while (cblist != NULL)
    {
        Cblist_item* item = cblist->first;
        while (item != NULL)
        {
            Constraint* constraint = item->constraints;
            while (constraint != NULL)
            {
                if (!Event_cache_add_event(cache, constraint->event_name))
                {
                    del_Event_cache(cache);
                    return NULL;
                }
                constraint = constraint->next;
            }
            item = item->next;
        }
        cblist = AAiter_get_next(iter);
    }

    return cache;
}


bool Bind_event_has_constraints(const Bind* map, Event_type event_type)
{
    rassert(map != NULL);

    Cblist* list = AAtree_get_exact(map->cblists, CBLIST_KEY(event_type));
    if (list == NULL)
        return false;

    Cblist_item* item = list->first;
    while (item != NULL)
    {
        if (item->constraints != NULL)
            return true;

        item = item->next;
    }

    return false;
}


Target_event* Bind_get_first(
        const Bind* map,
        const Event_names* event_names,
        Event_cache* cache,
        Env_state* estate,
        const char* event_name,
        const Value* value,
        Random* rand)
{
    rassert(map != NULL);
    rassert(cache != NULL);
    rassert(event_name != NULL);
    rassert(value != NULL);

    Event_cache_update(cache, event_name, value);

    const Event_type event_type = Event_names_get(event_names, event_name);

    Cblist* list = AAtree_get_exact(map->cblists, CBLIST_KEY(event_type));
    if (list == NULL)
        return NULL;

    Cblist_item* item = list->first;
    while (item != NULL)
    {
        //fprintf(stderr, "%d\n", __LINE__);
        Constraint* constraint = item->constraints;
        while (constraint != NULL)
        {
            if (!Constraint_match(constraint, cache, estate, rand))
                break;

            constraint = constraint->next;
        }
        //fprintf(stderr, "%d\n", __LINE__);
        if (constraint == NULL)
        {
            //fprintf(stderr, "item->events: %s\n", item->events->desc);
            return item->first_event;
        }
        //fprintf(stderr, "%d\n", __LINE__);
        item = item->next;
    }

    return NULL;
}


void del_Bind(Bind* map)
{
    if (map == NULL)
        return;

    del_AAtree(map->cblists);
    memory_free(map);

    return;
}


static bool Bind_dfs(
        const Bind* map, const Event_names* event_names, Event_type event_type);


static bool Bind_is_cyclic(const Bind* map, const Event_names* event_names)
{
    rassert(map != NULL);
    rassert(event_names != NULL);

    AAiter* iter = AAiter_init(AAITER_AUTO, map->cblists);
    Cblist* cblist = AAiter_get_at_least(iter, CBLIST_KEY(Event_NONE));
    while (cblist != NULL)
    {
        rassert(cblist->source_state != SOURCE_STATE_REACHED);
        if (cblist->source_state == SOURCE_STATE_VISITED)
        {
            cblist = AAiter_get_next(iter);
            continue;
        }

        rassert(cblist->source_state == SOURCE_STATE_NEW);
        if (Bind_dfs(map, event_names, cblist->event_type))
            return true;

        cblist = AAiter_get_next(iter);
    }

    return false;
}


static bool Bind_dfs(
        const Bind* map, const Event_names* event_names, Event_type event_type)
{
    rassert(map != NULL);
    rassert(event_names != NULL);

    Cblist* cblist = AAtree_get_exact(map->cblists, CBLIST_KEY(event_type));
    if (cblist == NULL || cblist->source_state == SOURCE_STATE_VISITED)
        return false;

    if (cblist->source_state == SOURCE_STATE_REACHED)
        return true;

    rassert(cblist->source_state == SOURCE_STATE_NEW);
    cblist->source_state = SOURCE_STATE_REACHED;

    Cblist_item* item = cblist->first;
    while (item != NULL)
    {
        Target_event* event = item->first_event;
        while (event != NULL)
        {
            Streader* sr = Streader_init(
                    STREADER_AUTO, event->desc, (int64_t)strlen(event->desc));
            char next_name[KQT_EVENT_NAME_MAX + 1] = "";
            Streader_readf(sr, "[%s", READF_STR(KQT_EVENT_NAME_MAX, next_name));
            rassert(!Streader_is_error_set(sr));

            const Event_type next_type = Event_names_get(event_names, next_name);

            if (Bind_dfs(map, event_names, next_type))
                return true;

            event = event->next;
        }

        item = item->next;
    }

    cblist->source_state = SOURCE_STATE_VISITED;

    return false;
}


static bool read_constraint(Streader* sr, int32_t index, void* userdata)
{
    rassert(sr != NULL);
    rassert(userdata != NULL);
    ignore(index);

    Cblist_item* item = userdata;

    Constraint* constraint = new_Constraint(sr);
    if (constraint == NULL)
        return false;

    constraint->next = item->constraints;
    item->constraints = constraint;

    return true;
}

static bool read_constraints(Streader* sr, Bind* map, Cblist_item* item)
{
    rassert(sr != NULL);
    rassert(map != NULL);
    rassert(item != NULL);

    return Streader_read_list(sr, read_constraint, item);
}


typedef struct edata
{
    Cblist_item* item;
    const Event_names* names;
} edata;

static bool read_event(Streader* sr, int32_t index, void* userdata)
{
    rassert(sr != NULL);
    rassert(userdata != NULL);
    ignore(index);

    edata* ed = userdata;

    Target_event* event = new_Target_event(sr, ed->names);
    if (event == NULL)
        return false;

    if (ed->item->last_event == NULL)
    {
        rassert(ed->item->first_event == NULL);
        ed->item->first_event = ed->item->last_event = event;
    }
    else
    {
        rassert(ed->item->first_event != NULL);
        ed->item->last_event->next = event;
        ed->item->last_event = event;
    }

    return true;
}

static bool read_events(
        Streader* sr, Cblist_item* item, const Event_names* names)
{
    rassert(sr != NULL);
    rassert(item != NULL);
    rassert(names != NULL);

    edata* ed = &(edata){ .item = item, .names = names, };

    return Streader_read_list(sr, read_event, ed);
}


static Cblist* new_Cblist(Event_type event_type)
{
    rassert(Event_is_valid(event_type));

    Cblist* list = memory_alloc_item(Cblist);
    if (list == NULL)
        return NULL;

    list->event_type = event_type;
    list->source_state = SOURCE_STATE_NEW;
    list->first = list->last = NULL;

    return list;
}


static int Cblist_cmp(const Cblist* c1, const Cblist* c2)
{
    rassert(c1 != NULL);
    rassert(c2 != NULL);

    if (c1->event_type < c2->event_type)
        return -1;
    else if (c1->event_type > c2->event_type)
        return 1;

    return 0;
}


static void Cblist_append(Cblist* list, Cblist_item* item)
{
    rassert(list != NULL);
    rassert(item != NULL);

    if (list->last == NULL)
    {
        rassert(list->first == NULL);
        list->first = list->last = item;
        return;
    }

    list->last->next = item;
    list->last = item;

    return;
}


static void del_Cblist(Cblist* list)
{
    if (list == NULL)
        return;

    Cblist_item* cur = list->first;
    while (cur != NULL)
    {
        Cblist_item* next = cur->next;
        del_Cblist_item(cur);
        cur = next;
    }

    memory_free(list);

    return;
}


static Cblist_item* new_Cblist_item(void)
{
    Cblist_item* item = memory_alloc_item(Cblist_item);
    if (item == NULL)
        return NULL;

    item->constraints = NULL;
    item->first_event = NULL;
    item->last_event = NULL;
    item->next = NULL;

    return item;
}


static void del_Cblist_item(Cblist_item* item)
{
    if (item == NULL)
        return;

    Constraint* curc = item->constraints;
    while (curc != NULL)
    {
        Constraint* nextc = curc->next;
        del_Constraint(curc);
        curc = nextc;
    }

    Target_event* curt = item->first_event;
    while (curt != NULL)
    {
        Target_event* nextt = curt->next;
        del_Target_event(curt);
        curt = nextt;
    }

    memory_free(item);

    return;
}


static Constraint* new_Constraint(Streader* sr)
{
    rassert(sr != NULL);

    if (Streader_is_error_set(sr))
        return NULL;

    Constraint* c = memory_alloc_item(Constraint);
    if (c == NULL)
    {
        Streader_set_memory_error(sr, "Could not allocate memory for bind");
        return NULL;
    }

    c->expr = NULL;
    c->next = NULL;

    if (!Streader_readf(sr, "[%s,", READF_STR(KQT_EVENT_NAME_MAX + 1, c->event_name)))
    {
        del_Constraint(c);
        return NULL;
    }

    Streader_skip_whitespace(sr);
    const char* const expr = Streader_get_remaining_data(sr);
    if (!Streader_read_string(sr, 0, NULL))
    {
        del_Constraint(c);
        return NULL;
    }

    const char* const expr_end = Streader_get_remaining_data(sr);

    if (!Streader_match_char(sr, ']'))
    {
        del_Constraint(c);
        return NULL;
    }

    rassert(expr_end != NULL);
    rassert(expr_end > expr);
    const ptrdiff_t len = expr_end - expr;

    c->expr = memory_calloc_items(char, len + 1);
    if (c->expr == NULL)
    {
        del_Constraint(c);
        Streader_set_memory_error(sr, "Could not allocate memory for bind");
        return NULL;
    }

    strncpy(c->expr, expr, (size_t)len);
    c->expr[len] = '\0';

    return c;
}


static bool Constraint_match(
        Constraint* constraint, Event_cache* cache, Env_state* estate, Random* rand)
{
    rassert(constraint != NULL);
    rassert(cache != NULL);
    rassert(estate != NULL);
    rassert(rand != NULL);

    const Value* value = Event_cache_get_value(cache, constraint->event_name);
    rassert(value != NULL);

    Value* result = VALUE_AUTO;
    Streader* sr = Streader_init(
            STREADER_AUTO, constraint->expr, (int64_t)strlen(constraint->expr));
    //fprintf(stderr, "%s, %s", constraint->event_name, constraint->expr);
    evaluate_expr(sr, estate, value, result, rand);
    //fprintf(stderr, ", %s", state->message);
    //fprintf(stderr, " -> %d %s\n", (int)result->type,
    //                               result->value.bool_type ? "true" : "false");

    return (result->type == VALUE_TYPE_BOOL) && result->value.bool_type;
}


static void del_Constraint(Constraint* constraint)
{
    if (constraint == NULL)
        return;

    memory_free(constraint->expr);
    memory_free(constraint);

    return;
}


static Target_event* new_Target_event(Streader* sr, const Event_names* names)
{
    rassert(sr != NULL);
    rassert(names != NULL);

    if (Streader_is_error_set(sr))
        return NULL;

    Target_event* event = memory_alloc_item(Target_event);
    if (event == NULL)
    {
        Streader_set_memory_error(sr, "Could not allocate memory for bind");
        return NULL;
    }

    event->ch_offset = 0;
    event->desc = NULL;
    event->next = NULL;

    int64_t ch_offset = 0;
    if (!Streader_readf(sr, "[%i,", &ch_offset))
    {
        del_Target_event(event);
        return NULL;
    }

    if (ch_offset <= -KQT_COLUMNS_MAX || ch_offset >= KQT_COLUMNS_MAX)
    {
        Streader_set_error(sr, "Channel offset out of bounds");
        del_Target_event(event);
        return NULL;
    }

    Streader_skip_whitespace(sr);
    const char* const desc = Streader_get_remaining_data(sr);

    char event_name[KQT_EVENT_NAME_MAX + 1] = "";
    if (!Streader_readf(sr, "[%s,", READF_STR(KQT_EVENT_NAME_MAX + 1, event_name)))
    {
        del_Target_event(event);
        return NULL;
    }

    if (Event_names_get(names, event_name) == Event_NONE)
    {
        Streader_set_error(sr, "Unsupported event type: %s", event_name);
        del_Target_event(event);
        return NULL;
    }

    Value_type type = Event_names_get_param_type(names, event_name);
    if (type == VALUE_TYPE_NONE)
        Streader_read_null(sr);
    else
        Streader_read_string(sr, 0, NULL);

    if (!Streader_readf(sr, "]]"))
    {
        del_Target_event(event);
        return NULL;
    }

    const char* const desc_end = Streader_get_remaining_data(sr);
    if (desc_end == NULL)
    {
        Streader_set_error(sr, "Unexpected end of data");
        del_Target_event(event);
        return NULL;
    }

    const ptrdiff_t len = desc_end - desc;
    rassert(len > 0);
    event->desc = memory_alloc_items(char, len + 1);
    if (event->desc == NULL)
    {
        Streader_set_memory_error(sr, "Could not allocate memory for bind");
        del_Target_event(event);
        return NULL;
    }

    event->ch_offset = (int)ch_offset;
    memcpy(event->desc, desc, (size_t)len);
    event->desc[len] = '\0';

    return event;
}


static void del_Target_event(Target_event* event)
{
    if (event == NULL)
        return;

    memory_free(event->desc);
    memory_free(event);

    return;
}


