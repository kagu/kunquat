

/*
 * Author: Tomi Jylhä-Ollila, Finland 2013-2016
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <player/Device_states.h>

#include <containers/AAtree.h>
#include <debug/assert.h>
#include <init/Connections.h>
#include <init/devices/Audio_unit.h>
#include <kunquat/limits.h>
#include <player/devices/Device_state.h>
#include <player/devices/Device_thread_state.h>
#include <memory.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


struct Device_states
{
    AAtree* states;
    AAtree* thread_states[KQT_THREADS_MAX];
};


Device_states* new_Device_states(void)
{
    Device_states* states = memory_alloc_item(Device_states);
    if (states == NULL)
        return NULL;

    states->states = NULL;
    for (int i = 0; i < KQT_THREADS_MAX; ++i)
        states->thread_states[i] = NULL;

    states->states = new_AAtree(
            (AAtree_item_cmp*)Device_state_cmp, (AAtree_item_destroy*)del_Device_state);
    if (states->states == NULL)
    {
        del_Device_states(states);
        return NULL;
    }

    if (!Device_states_set_thread_count(states, 1))
    {
        del_Device_states(states);
        return NULL;
    }

    return states;
}


bool Device_states_set_thread_count(Device_states* states, int new_count)
{
    rassert(states != NULL);
    rassert(new_count >= 1);
    rassert(new_count <= KQT_THREADS_MAX);

    // Create new containers
    for (int i = 0; i < new_count; ++i)
    {
        if (states->thread_states[i] == NULL)
        {
            AAtree* thread_states = new_AAtree(
                    (AAtree_item_cmp*)Device_thread_state_cmp,
                    (AAtree_item_destroy*)del_Device_thread_state);
            if (thread_states == NULL)
                return false;

            states->thread_states[i] = thread_states;
        }
    }

    // Create new thread states for existing Device states
    {
        AAiter* iter = AAiter_init(AAITER_AUTO, states->states);
        const Device_state* state = AAiter_get_at_least(iter, DEVICE_STATE_KEY(0));
        while (state != NULL)
        {
            const uint32_t device_id = state->device_id;
            const int32_t audio_buffer_size = state->audio_buffer_size;

            const Device_thread_state* tkey = DEVICE_THREAD_STATE_KEY(device_id);

            for (int i = 0; i < KQT_THREADS_MAX; ++i)
            {
                AAtree* thread_states = states->thread_states[i];
                if ((thread_states != NULL) && !AAtree_contains(thread_states, tkey))
                {
                    Device_thread_state* ts =
                        new_Device_thread_state(device_id, audio_buffer_size);
                    if ((ts == NULL) || !AAtree_ins(thread_states, ts))
                    {
                        del_Device_thread_state(ts);
                        return false;
                    }
                }
            }

            state = AAiter_get_next(iter);
        }
    }

    // Remove excess thread state collections
    for (int i = new_count; i < KQT_THREADS_MAX; ++i)
    {
        del_AAtree(states->thread_states[i]);
        states->thread_states[i] = NULL;
    }

    return true;
}


bool Device_states_add_state(Device_states* states, Device_state* state)
{
    rassert(states != NULL);
    rassert(state != NULL);
    rassert(!AAtree_contains(states->states, state));

    if (!AAtree_ins(states->states, state))
        return false;

    const int32_t audio_buffer_size = state->audio_buffer_size;

    for (int i = 0; i < KQT_THREADS_MAX; ++i)
    {
        AAtree* thread_states = states->thread_states[i];
        if (thread_states == NULL)
            continue;

        Device_thread_state* ts =
            new_Device_thread_state(state->device_id, audio_buffer_size);
        if ((ts == NULL) || !AAtree_ins(thread_states, ts))
        {
            del_Device_thread_state(ts);
            return false;
        }
    }

    Device_state_reset(state);

    return true;
}


Device_state* Device_states_get_state(const Device_states* states, uint32_t id)
{
    rassert(states != NULL);
    rassert(id > 0);

    const Device_state* key = DEVICE_STATE_KEY(id);
    rassert(AAtree_contains(states->states, key));

    return AAtree_get_exact(states->states, key);
}


void Device_states_remove_state(Device_states* states, uint32_t id)
{
    rassert(states != NULL);
    rassert(id > 0);

    const Device_state* key = DEVICE_STATE_KEY(id);
    del_Device_state(AAtree_remove(states->states, key));

    const Device_thread_state* tkey = DEVICE_THREAD_STATE_KEY(id);

    for (int i = 0; i < KQT_THREADS_MAX; ++i)
    {
        AAtree* thread_states = states->thread_states[i];
        if (thread_states != NULL)
            del_Device_thread_state(AAtree_remove(thread_states, tkey));
    }

    return;
}


Device_thread_state* Device_states_get_thread_state(
        const Device_states* states, int thread_id, uint32_t device_id)
{
    rassert(states != NULL);
    rassert(thread_id >= 0);
    rassert(thread_id < KQT_THREADS_MAX);
    rassert(device_id > 0);

    AAtree* thread_states = states->thread_states[thread_id];
    rassert(thread_states != NULL);

    const Device_thread_state* key = DEVICE_THREAD_STATE_KEY(device_id);
    Device_thread_state* ts = AAtree_get_exact(thread_states, key);
    rassert(ts != NULL);

    return ts;
}


static bool Device_states_add_audio_buffer(
        Device_states* states, uint32_t device_id, Device_port_type type, int port)
{
    rassert(states != NULL);
    rassert(device_id > 0);
    rassert(type < DEVICE_PORT_TYPES);
    rassert(port >= 0);
    rassert(port < KQT_DEVICE_PORTS_MAX);

    Device_state* dstate = Device_states_get_state(states, device_id);
    if (!Device_state_add_audio_buffer(dstate, type, port))
        return false;

    const bool add_voice_buffers = !Device_get_mixed_signals(dstate->device);

    for (int i = 0; i < KQT_THREADS_MAX; ++i)
    {
        AAtree* thread_states = states->thread_states[i];
        if (thread_states == NULL)
            continue;

        Device_thread_state* ts = Device_states_get_thread_state(states, i, device_id);
        if (!Device_thread_state_add_mixed_buffer(ts, type, port))
            return false;

        if (add_voice_buffers && !Device_thread_state_add_voice_buffer(ts, type, port))
            return false;
    }

    return true;
}


bool Device_states_set_audio_rate(Device_states* states, int32_t rate)
{
    rassert(states != NULL);
    rassert(rate > 0);

    AAiter* iter = AAiter_init(AAITER_AUTO, states->states);

    Device_state* ds = AAiter_get_at_least(iter, DEVICE_STATE_KEY(0));

    while (ds != NULL)
    {
        if (!Device_state_set_audio_rate(ds, rate))
            return false;

        ds = AAiter_get_next(iter);
    }

    return true;
}


bool Device_states_set_audio_buffer_size(Device_states* states, int32_t size)
{
    rassert(states != NULL);
    rassert(size >= 0);

    {
        AAiter* iter = AAiter_init(AAITER_AUTO, states->states);

        Device_state* ds = AAiter_get_at_least(iter, DEVICE_STATE_KEY(0));

        while (ds != NULL)
        {
            if (!Device_state_set_audio_buffer_size(ds, size))
                return false;

            ds = AAiter_get_next(iter);
        }
    }

    for (int i = 0; i < KQT_THREADS_MAX; ++i)
    {
        AAtree* thread_states = states->thread_states[i];
        if (thread_states == NULL)
            continue;

        AAiter* iter = AAiter_init(AAITER_AUTO, thread_states);

        Device_thread_state* ts = AAiter_get_at_least(iter, DEVICE_THREAD_STATE_KEY(0));
        while (ts != NULL)
        {
            if (!Device_thread_state_set_audio_buffer_size(ts, size))
                return false;

            ts = AAiter_get_next(iter);
        }
    }

    return true;
}


/*
bool Device_states_allocate_space(Device_states* states, char* key)
{
    rassert(states != NULL);
    rassert(key != NULL);

    AAiter* iter = AAiter_init(AAITER_AUTO, states->states);

    Device_state* ds = AAiter_get_at_least(iter, DEVICE_STATE_KEY(0));

    while (ds != NULL)
    {
        if (!Device_state_allocate_space(ds, key))
            return false;

        ds = AAiter_get_next(iter);
    }

    return true;
}
// */


void Device_states_clear_audio_buffers(
        Device_states* states, int32_t start, int32_t stop)
{
    rassert(states != NULL);

    for (int i = 0; i < KQT_THREADS_MAX; ++i)
    {
        AAtree* thread_states = states->thread_states[i];
        if (thread_states == NULL)
            continue;

        AAiter* iter = AAiter_init(AAITER_AUTO, thread_states);

        Device_thread_state* ts = AAiter_get_at_least(iter, DEVICE_THREAD_STATE_KEY(0));
        while (ts != NULL)
        {
            Device_thread_state_clear_mixed_buffers(ts, start, stop);

            ts = AAiter_get_next(iter);
        }
    }

    return;
}


void Device_states_set_tempo(Device_states* states, double tempo)
{
    rassert(states != NULL);
    rassert(isfinite(tempo));
    rassert(tempo > 0);

    AAiter* iter = AAiter_init(AAITER_AUTO, states->states);

    Device_state* ds = AAiter_get_at_least(iter, DEVICE_STATE_KEY(0));

    while (ds != NULL)
    {
        Device_state_set_tempo(ds, tempo);
        ds = AAiter_get_next(iter);
    }

    return;
}


static bool init_buffers(Device_states* dstates, const Device_node* node)
{
    rassert(dstates != NULL);
    rassert(node != NULL);

    const Device* node_device = Device_node_get_device(node);
    if (node_device == NULL)
        return true;

    Device_thread_state* node_ts =
        Device_states_get_thread_state(dstates, 0, Device_get_id(node_device));
    rassert(Device_thread_state_get_node_state(node_ts) != DEVICE_NODE_STATE_REACHED);

    if (Device_thread_state_get_node_state(node_ts) == DEVICE_NODE_STATE_VISITED)
        return true;

    Device_thread_state_set_node_state(node_ts, DEVICE_NODE_STATE_REACHED);

    for (int port = 0; port < KQT_DEVICE_PORTS_MAX; ++port)
    {
        const Connection* edge = Device_node_get_received(node, port);
        while (edge != NULL)
        {
            rassert(edge->node != NULL);
            const Device* send_device = Device_node_get_device(edge->node);
            if (send_device == NULL ||
                    !Device_has_complete_type(send_device) ||
                    !Device_get_port_existence(
                        node_device, DEVICE_PORT_TYPE_RECV, port) ||
                    !Device_get_port_existence(
                        send_device, DEVICE_PORT_TYPE_SEND, edge->port))
            {
                edge = edge->next;
                continue;
            }
            /*
            if (!Device_get_port_existence(
                    node_device, DEVICE_PORT_TYPE_RECV, port))
                fprintf(stderr, "Warning: connecting to non-existent port %d of device %s\n",
                        port, node->name);
            if (!Device_get_port_existence(
                    send_device, DEVICE_PORT_TYPE_SEND, edge->port))
                fprintf(stderr, "Warning: connecting from non-existent port %d of device %s\n",
                        edge->port, edge->node->name);
            // */

            // Add receive buffers
            const uint32_t recv_id = Device_get_id(node_device);
            if (!Device_states_add_audio_buffer(
                        dstates, recv_id, DEVICE_PORT_TYPE_RECV, port))
                return false;

            // Add send buffers
            const uint32_t send_id = Device_get_id(send_device);
            if (!Device_states_add_audio_buffer(
                        dstates, send_id, DEVICE_PORT_TYPE_SEND, edge->port))
                return false;

            // Recurse to the sender
            if (!init_buffers(dstates, edge->node))
                return false;

            edge = edge->next;
        }
    }

    Device_thread_state_set_node_state(node_ts, DEVICE_NODE_STATE_VISITED);
    return true;
}


static bool init_effect_buffers(Device_states* dstates, const Device_node* node)
{
    rassert(dstates != NULL);
    rassert(node != NULL);

    const Device* node_device = Device_node_get_device(node);
    if (node_device == NULL)
        return true;

    Device_thread_state* node_ts =
        Device_states_get_thread_state(dstates, 0, Device_get_id(node_device));

    if (Device_thread_state_get_node_state(node_ts) > DEVICE_NODE_STATE_NEW)
    {
        rassert(Device_thread_state_get_node_state(node_ts) != DEVICE_NODE_STATE_REACHED);
        return true;
    }

    Device_thread_state_set_node_state(node_ts, DEVICE_NODE_STATE_REACHED);

    if (Device_node_get_type(node) == DEVICE_NODE_TYPE_AU)
    {
        const Audio_unit* au = Device_node_get_au_mut(node);
        if (au == NULL)
        {
            Device_thread_state_set_node_state(node_ts, DEVICE_NODE_STATE_VISITED);
            return true;
        }

        const Connections* au_conns = Audio_unit_get_connections(au);
        if (au_conns != NULL)
        {
            if (!Device_states_prepare(dstates, au_conns))
                return false;
        }
    }

    for (int port = 0; port < KQT_DEVICE_PORTS_MAX; ++port)
    {
        const Connection* edge = Device_node_get_received(node, port);
        while (edge != NULL)
        {
            if (Device_node_get_device(edge->node) == NULL)
            {
                edge = edge->next;
                continue;
            }

            if (!init_effect_buffers(dstates, edge->node))
                return false;

            edge = edge->next;
        }
    }

    Device_thread_state_set_node_state(node_ts, DEVICE_NODE_STATE_VISITED);
    return true;
}


static bool Device_states_init_buffers(Device_states* dstates, const Connections* conns)
{
    rassert(dstates != NULL);
    rassert(conns != NULL);

    const Device_node* master = Connections_get_master(conns);
    rassert(master != NULL);
    Device_states_reset_node_states(dstates);
    if (!init_buffers(dstates, master))
        return false;

    Device_states_reset_node_states(dstates);
    return init_effect_buffers(dstates, master);
}


bool Device_states_prepare(Device_states* dstates, const Connections* conns)
{
    rassert(dstates != NULL);
    rassert(conns != NULL);

    return Device_states_init_buffers(dstates, conns);
}


void Device_states_mix_thread_states(
        Device_states* dstates, int32_t buf_start, int32_t buf_stop)
{
    rassert(dstates != NULL);
    rassert(buf_start >= 0);
    rassert(buf_stop >= 0);

    AAiter* iter = AAiter_init(AAITER_AUTO, dstates->thread_states[0]);
    Device_thread_state* dest_state =
        AAiter_get_at_least(iter, DEVICE_THREAD_STATE_KEY(0));

    while (dest_state != NULL)
    {
        const uint32_t device_id = dest_state->device_id;
        const Device_thread_state* tkey = DEVICE_THREAD_STATE_KEY(device_id);

        for (int thread_id = 1; thread_id < KQT_THREADS_MAX; ++thread_id)
        {
            AAtree* thread_states = dstates->thread_states[thread_id];
            if (thread_states == NULL)
                continue;

            const Device_thread_state* src_state =
                AAtree_get_exact(thread_states, tkey);
            if (Device_thread_state_has_mixed_audio(src_state))
            {
                for (int port = 0; port < KQT_DEVICE_PORTS_MAX; ++port)
                {
                    Work_buffer* dest_buffer = Device_thread_state_get_mixed_buffer(
                            dest_state, DEVICE_PORT_TYPE_SEND, port);
                    if (dest_buffer == NULL)
                        continue;

                    const Work_buffer* src_buffer = Device_thread_state_get_mixed_buffer(
                            src_state, DEVICE_PORT_TYPE_SEND, port);
                    rassert(src_buffer != NULL);

                    Work_buffer_mix(dest_buffer, src_buffer, buf_start, buf_stop);
                }
            }
        }

        dest_state = AAiter_get_next(iter);
    }

    return;
}


static void process_mixed_signals(
        Device_states* dstates,
        const Device_node* node,
        const Work_buffers* wbs,
        int32_t buf_start,
        int32_t buf_stop,
        int32_t audio_rate,
        double tempo)
{
    rassert(dstates != NULL);
    rassert(node != NULL);
    rassert(wbs != NULL);
    rassert(buf_start >= 0);
    rassert(buf_stop >= 0);
    rassert(audio_rate > 0);
    rassert(isfinite(tempo));
    rassert(tempo > 0);

    const Device* node_device = Device_node_get_device(node);
    if ((node_device == NULL) || !Device_is_existent(node_device))
        return;

    Device_thread_state* node_ts =
        Device_states_get_thread_state(dstates, 0, Device_get_id(node_device));

    //fprintf(stderr, "Entering node %p %s\n", (void*)node, node->name);
    if (Device_thread_state_get_node_state(node_ts) > DEVICE_NODE_STATE_NEW)
    {
        rassert(Device_thread_state_get_node_state(node_ts) == DEVICE_NODE_STATE_VISITED);
        return;
    }

    Device_thread_state_set_node_state(node_ts, DEVICE_NODE_STATE_REACHED);

    for (int port = 0; port < KQT_DEVICE_PORTS_MAX; ++port)
    {
        const Connection* edge = Device_node_get_received(node, port);

        if (edge != NULL)
            Device_thread_state_mark_input_port_connected(node_ts, port);

        while (edge != NULL)
        {
            const Device* send_device = Device_node_get_device(edge->node);
            if (send_device == NULL)
            {
                edge = edge->next;
                continue;
            }

            Device_thread_state* send_ts =
                Device_states_get_thread_state(dstates, 0, Device_get_id(send_device));

            process_mixed_signals(
                    dstates, edge->node, wbs, buf_start, buf_stop, audio_rate, tempo);

            Work_buffer* send = Device_thread_state_get_mixed_buffer(
                    send_ts, DEVICE_PORT_TYPE_SEND, edge->port);
            Work_buffer* receive = Device_thread_state_get_mixed_buffer(
                    node_ts, DEVICE_PORT_TYPE_RECV, port);
            if (receive == NULL || send == NULL)
            {
                /*
                if (receive != NULL)
                {
                    fprintf(stderr, "receive %p of %p %s, but no send from %p!\n",
                            (void*)receive, (void*)node, node->name, (void*)edge->node);
                }
                else if (send != NULL)
                {
                    fprintf(stderr, "send %p, but no receive!\n", (void*)send);
                }
                // */
                edge = edge->next;
                continue;
            }

            /*
            fprintf(stderr, "%s %d %.1f\n",
                    edge->node->name,
                    (int)Device_get_id((const Device*)send_device),
                    Work_buffer_get_contents(send)[0]);
            // */
            Work_buffer_mix(receive, send, buf_start, buf_stop);

            edge = edge->next;
        }
    }

    //fprintf(stderr, "Rendering mixed on %p %s\n", (void*)node, node->name);
    Device_state* node_dstate =
        Device_states_get_state(dstates, Device_get_id(node_device));
    Device_state_render_mixed(node_dstate, node_ts, wbs, buf_start, buf_stop, tempo);

    Device_thread_state_set_node_state(node_ts, DEVICE_NODE_STATE_VISITED);
    return;
}


void Device_states_process_mixed_signals(
        Device_states* dstates,
        bool hack_reset,
        const Connections* conns,
        const Work_buffers* wbs,
        int32_t buf_start,
        int32_t buf_stop,
        int32_t audio_rate,
        double tempo)
{
    rassert(dstates != NULL);
    rassert(conns != NULL);
    rassert(wbs != NULL);
    rassert(buf_start >= 0);
    rassert(audio_rate > 0);
    rassert(isfinite(tempo));
    rassert(tempo > 0);

    const Device_node* master = Connections_get_master(conns);
    rassert(master != NULL);
    if (buf_start >= buf_stop)
        return;

#if 0
    static bool called = false;
    if (!called)
    {
        Connections_print(graph, stderr);
    }
    called = true;
//    fprintf(stderr, "Mix process:\n");
#endif

    if (hack_reset)
        Device_states_reset_node_states(dstates);

    process_mixed_signals(
            dstates, master, wbs, buf_start, buf_stop, audio_rate, tempo);

    return;
}


void Device_states_reset(Device_states* states)
{
    rassert(states != NULL);

    AAiter* iter = AAiter_init(AAITER_AUTO, states->states);

    Device_state* ds = AAiter_get_at_least(iter, DEVICE_STATE_KEY(0));

    while (ds != NULL)
    {
        Device_state_reset(ds);
        ds = AAiter_get_next(iter);
    }

    return;
}


void Device_states_reset_node_states(Device_states* states)
{
    rassert(states != NULL);

    for (int i = 0; i < KQT_THREADS_MAX; ++i)
    {
        AAtree* thread_states = states->thread_states[i];
        if (thread_states == NULL)
            continue;

        AAiter* iter = AAiter_init(AAITER_AUTO, thread_states);

        Device_thread_state* ts = AAiter_get_at_least(iter, DEVICE_THREAD_STATE_KEY(0));

        while (ts != NULL)
        {
            Device_thread_state_set_node_state(ts, DEVICE_NODE_STATE_NEW);
            ts = AAiter_get_next(iter);
        }
    }

    return;
}


void del_Device_states(Device_states* states)
{
    if (states == NULL)
        return;

    del_AAtree(states->states);

    for (int i = 0; i < KQT_THREADS_MAX; ++i)
        del_AAtree(states->thread_states[i]);

    memory_free(states);

    return;
}


