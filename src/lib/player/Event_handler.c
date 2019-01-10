

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


#include <player/Event_handler.h>

#include <debug/assert.h>
#include <init/Au_table.h>
#include <init/Bind.h>
#include <init/devices/Audio_unit.h>
#include <init/devices/Processor.h>
#include <init/Module.h>
#include <kunquat/limits.h>
#include <memory.h>
#include <player/Channel.h>
#include <player/Event_names.h>
#include <player/Event_properties.h>
#include <player/Event_type.h>
#include <player/events/Event_au_decl.h>
#include <player/events/Event_channel_decl.h>
#include <player/events/Event_control_decl.h>
#include <player/events/Event_general_decl.h>
#include <player/events/Event_master_decl.h>
#include <player/events/Event_params.h>
#include <player/General_state.h>
#include <string/common.h>
#include <string/Streader.h>
#include <Value.h>

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct Event_handler
{
    Channel* channels[KQT_COLUMNS_MAX];
    Device_states* device_states;
    Master_params* master_params;
    Au_table* au_table;
    Event_properties* event_props;
    Event_names* event_names;

    Event_control_interface* control_process[Event_control_STOP];
    Event_general_interface* general_process[Event_general_STOP];
    Event_channel_interface* ch_process[Event_channel_STOP];
    Event_master_interface* master_process[Event_master_STOP];
    Event_au_interface* au_process[Event_au_STOP];
};


Event_handler* new_Event_handler(
        Master_params* master_params,
        Channel** channels,
        Device_states* device_states,
        Au_table* au_table)
{
    rassert(master_params != NULL);
    rassert(channels != NULL);
    rassert(device_states != NULL);
    rassert(au_table != NULL);

    Event_handler* eh = memory_alloc_item(Event_handler);
    if (eh == NULL)
        return NULL;

    eh->event_props = NULL;
    eh->event_names = NULL;

    eh->event_props = new_Event_properties();
    if (eh->event_props == NULL)
    {
        del_Event_handler(eh);
        return NULL;
    }

    eh->event_names = new_Event_names(eh->event_props);
    if (eh->event_names == NULL)
    {
        del_Event_handler(eh);
        return NULL;
    }

    eh->master_params = master_params;
    for (int i = 0; i < KQT_CHANNELS_MAX; ++i)
        eh->channels[i] = channels[i];
    eh->device_states = device_states;
    eh->au_table = au_table;

#define EVENT_CONTROL_DEF(name, type_suffix, arg_type, validator)                \
        Event_handler_set_control_process(                                       \
        eh, Event_control_##type_suffix, Event_control_##type_suffix##_process);
#include <player/events/Event_control_types.h>

#define EVENT_GENERAL_DEF(name, type_suffix, arg_type, validator)                \
        Event_handler_set_general_process(                                       \
        eh, Event_general_##type_suffix, Event_general_##type_suffix##_process);
#include <player/events/Event_general_types.h>

#define EVENT_MASTER_DEF(name, type_suffix, arg_type, validator)               \
        Event_handler_set_master_process(                                      \
        eh, Event_master_##type_suffix, Event_master_##type_suffix##_process);
#include <player/events/Event_master_types.h>

#define EVENT_CHANNEL_DEF(name, type_suffix, arg_type, validator)                \
        Event_handler_set_ch_process(                                            \
        eh, Event_channel_##type_suffix, Event_channel_##type_suffix##_process);
#include <player/events/Event_channel_types.h>

#define EVENT_AU_DEF(name, type_suffix, arg_type, validator)           \
        Event_handler_set_au_process(                                  \
        eh, Event_au_##type_suffix, Event_au_##type_suffix##_process);
#include <player/events/Event_au_types.h>

    return eh;
}


const Event_names* Event_handler_get_names(const Event_handler* eh)
{
    rassert(eh != NULL);
    return eh->event_names;
}


bool Event_handler_set_ch_process(
        Event_handler* eh, Event_type type, Event_channel_interface* ch_process)
{
    rassert(eh != NULL);
    rassert(Event_is_channel(type));
    rassert(ch_process != NULL);

    eh->ch_process[type] = ch_process;

    return true;
}


bool Event_handler_set_general_process(
        Event_handler* eh, Event_type type, Event_general_interface* general_process)
{
    rassert(eh != NULL);
    rassert(Event_is_general(type));
    rassert(general_process != NULL);

    eh->general_process[type] = general_process;

    return true;
}


bool Event_handler_set_control_process(
        Event_handler* eh, Event_type type, Event_control_interface* control_process)
{
    rassert(eh != NULL);
    rassert(Event_is_control(type));
    rassert(control_process != NULL);

    eh->control_process[type] = control_process;

    return true;
}


bool Event_handler_set_master_process(
        Event_handler* eh, Event_type type, Event_master_interface* global_process)
{
    rassert(eh != NULL);
    rassert(Event_is_master(type));
    rassert(global_process != NULL);

    eh->master_process[type] = global_process;

    return true;
}


bool Event_handler_set_au_process(
        Event_handler* eh, Event_type type, Event_au_interface* au_process)
{
    rassert(eh != NULL);
    rassert(Event_is_au(type));
    rassert(au_process != NULL);

    eh->au_process[type] = au_process;

    return true;
}


static bool Event_handler_handle(
        Event_handler* eh, int index, Event_type type, const Value* value, bool external)
{
    rassert(eh != NULL);
    rassert(index >= 0);
    rassert(index < KQT_COLUMNS_MAX);
    rassert(Event_is_valid(type));
    rassert(eh->channels[index]->audio_rate > 0);
    rassert(eh->channels[index]->tempo > 0);

    Event_params* params = EVENT_PARAMS_AUTO;
    params->external = external;
    params->arg = value;

    if (Event_is_channel(type))
    {
        if (eh->ch_process[type] == NULL)
            return false;

        return eh->ch_process[type](
                eh->channels[index],
                eh->device_states,
                eh->master_params,
                params);
    }
    else if (Event_is_au(type))
    {
        // Find our audio unit
        Audio_unit* au = Module_get_au_from_input(
                eh->master_params->parent.module,
                eh->channels[index]->au_input);
        if (au == NULL)
            return false;

        const Au_params* au_params = Audio_unit_get_params(au);
        rassert(au_params != NULL);
        Au_state* au_state = (Au_state*)Device_states_get_state(
                eh->device_states,
                Device_get_id((Device*)au));

        return eh->au_process[type](
                au,
                au_params,
                au_state,
                eh->master_params,
                eh->channels[index],
                eh->device_states,
                params);
    }
    else if (Event_is_master(type))
    {
        if (eh->master_process[type] == NULL)
            return false;

        return eh->master_process[type](eh->master_params, params);
    }
    else if (Event_is_control(type))
    {
        return eh->control_process[type](
                (General_state*)eh->master_params, eh->channels[index], params);
    }
    else if (Event_is_general(type))
    {
        General_state* gstate = (General_state*)eh->channels[index];
        return eh->general_process[type](gstate, params);
    }

    return false;
}


bool Event_handler_trigger_by_type(
        Event_handler* eh, int ch_num, Event_type type, const Value* arg, bool external)
{
    rassert(eh != NULL);
    rassert(ch_num >= 0);
    rassert(ch_num < KQT_CHANNELS_MAX);
    rassert(type != Event_NONE);
    rassert(!Event_is_query(type));
    rassert(!Event_is_auto(type));
    rassert(arg != NULL);

    Param_validator* validator =
        Event_properties_get_param_validator(eh->event_props, type);
    if ((validator != NULL) && !validator(arg))
    {
        // TODO: proper warning system
        //fprintf(stdout, "Invalid argument for event %s\n", name);
        return false;
    }

    rassert(eh->channels[ch_num]->audio_rate > 0);
    rassert(eh->channels[ch_num]->tempo > 0);

    return Event_handler_handle(eh, ch_num, type, arg, external);
}


bool Event_handler_trigger(
        Event_handler* eh, int ch_num, const char* name, const Value* arg, bool external)
{
    rassert(eh != NULL);
    rassert(ch_num >= 0);
    rassert(ch_num < KQT_CHANNELS_MAX);
    rassert(name != NULL);
    rassert(arg != NULL);

    const Event_type type = Event_names_get(eh->event_names, name);

    return Event_handler_trigger_by_type(eh, ch_num, type, arg, external);
}


#if 0
bool Event_handler_process_type(
        Event_handler* eh,
        int index,
        char** desc,
        char* event_name,
        Event_type* event_type,
        Read_state* state)
{
    rassert(eh != NULL);
    rassert(index >= 0);
    rassert(index < KQT_COLUMNS_MAX);
    rassert(desc != NULL);
    rassert(*desc != NULL);
    rassert(event_name != NULL);
    rassert(event_type != NULL);
    rassert(state != NULL);
    rassert(!state->error);

    *desc = read_const_char(*desc, '[', state);
    *desc = read_string(*desc, event_name, EVENT_NAME_MAX + 2, state);
    *desc = read_const_char(*desc, ',', state);
    if (state->error)
        return false;

    *event_type = Event_names_get(eh->event_names, event_name);
    if (*event_type == Event_NONE)
    {
        Read_state_set_error(
                state,
                "Unsupported event type: %s",
                event_name);
        return false;
    }

    rassert(Event_is_valid(*event_type));
    if (!General_state_events_enabled((General_state*)eh->channels[index]) &&
            *event_type != Event_general_if &&
            *event_type != Event_general_else &&
            *event_type != Event_general_end_if)
        return false;

    return true;
}
#endif


void del_Event_handler(Event_handler* eh)
{
    if (eh == NULL)
        return;

    del_Event_names(eh->event_names);
    del_Event_properties(eh->event_props);
    memory_free(eh);

    return;
}


