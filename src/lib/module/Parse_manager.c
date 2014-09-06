

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


#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#include <Connections.h>
#include <debug/assert.h>
#include <devices/Device_event_keys.h>
#include <devices/Device_params.h>
#include <devices/dsps/DSP_type.h>
#include <devices/generators/Gen_type.h>
#include <Handle_private.h>
#include <memory.h>
#include <module/Bind.h>
#include <module/Environment.h>
#include <module/manifest.h>
#include <module/Parse_manager.h>
#include <string/common.h>
#include <string/key_pattern.h>
#include <string/Streader.h>


#define READ(name) static bool read_##name( \
        Handle* handle,                     \
        Module* module,                     \
        const Key_indices indices,          \
        const char* subkey,                 \
        Streader* sr)


#define MODULE_KEYP(name, keyp, def) READ(name);
#include <module/Module_key_patterns.h>


static const struct
{
    const char* keyp;
    bool (*func)(Handle*, Module*, const Key_indices, const char*, Streader*);
} keyp_to_func[] =
{
#define MODULE_KEYP(name, keyp, def) { keyp, read_##name, },
#include <module/Module_key_patterns.h>
    { NULL, NULL }
};


#define set_error(handle, sr)                                               \
    if (true)                                                               \
    {                                                                       \
        if (Error_get_type(&(sr)->error) == ERROR_FORMAT)                   \
            Handle_set_validation_error_from_Error((handle), &(sr)->error); \
        else                                                                \
            Handle_set_error_from_Error((handle), &(sr)->error);            \
    } else (void)0


static bool prepare_connections(Handle* handle)
{
    assert(handle != NULL);

    Module* module = Handle_get_module(handle);
    Connections* graph = module->connections;

    if (graph == NULL)
        return true;

    Device_states* states = Player_get_device_states(handle->player);

    if (!Connections_prepare(graph, states))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory for connections");
        return false;
    }

    return true;
}


bool parse_data(
        Handle* handle,
        const char* key,
        const void* data,
        long length)
{
//    fprintf(stderr, "parsing %s\n", key);
    assert(handle != NULL);
    check_key(handle, key, false);
    assert(data != NULL || length == 0);
    assert(length >= 0);

    if (length == 0)
    {
        data = NULL;
    }

    // Get key pattern info
    char key_pattern[KQT_KEY_LENGTH_MAX] = "";
    Key_indices key_indices = { 0 };
    for (int i = 0; i < KEY_INDICES_MAX; ++i)
        key_indices[i] = -1;

    if (!extract_key_pattern(key, key_pattern, key_indices))
    {
        fprintf(stderr, "invalid key: %s\n", key);
        assert(false);
        return false;
    }

    assert(strlen(key) == strlen(key_pattern));

    // Find a known key pattern that is a prefix of our retrieved key pattern
    for (int i = 0; keyp_to_func[i].keyp != NULL; ++i)
    {
        if (string_has_prefix(key_pattern, keyp_to_func[i].keyp))
        {
            // Found a match
            const char* subkey = key + strlen(keyp_to_func[i].keyp);

            Streader* sr = Streader_init(STREADER_AUTO, data, length);

            return keyp_to_func[i].func(
                    handle,
                    Handle_get_module(handle),
                    key_indices,
                    subkey,
                    sr);
        }
    }

    // Accept unknown key pattern without modification
    return true;
}


READ(composition)
{
    (void)indices;
    (void)subkey;

    if (!Module_parse_composition(module, sr))
    {
        set_error(handle, sr);
        return false;
    }

    return true;
}


READ(connections)
{
    (void)indices;
    (void)subkey;

    Connections* graph = new_Connections_from_string(
            sr,
            CONNECTION_LEVEL_GLOBAL,
            Module_get_insts(module),
            Module_get_effects(module),
            NULL,
            (Device*)module);
    if (graph == NULL)
    {
        set_error(handle, sr);
        return false;
    }

    if (module->connections != NULL)
        del_Connections(module->connections);

    module->connections = graph;

    if (!prepare_connections(handle))
        return false;

    return true;
}


READ(control_map)
{
    (void)indices;
    (void)subkey;

    if (!Module_set_ins_map(module, sr))
    {
        set_error(handle, sr);
        return false;
    }

    return true;
}


READ(control_manifest)
{
    (void)subkey;

    const int32_t index = indices[0];

    if (index < 0 || index >= KQT_CONTROLS_MAX)
        return true;

    const bool existent = read_default_manifest(sr);
    if (Streader_is_error_set(sr))
    {
        set_error(handle, sr);
        return false;
    }

    Module_set_control(module, index, existent);

    return true;
}


READ(random_seed)
{
    (void)indices;
    (void)subkey;

    if (!Module_parse_random_seed(module, sr))
    {
        set_error(handle, sr);
        return false;
    }

    return true;
}


READ(environment)
{
    (void)indices;
    (void)subkey;

    if (!Environment_parse(module->env, sr))
    {
        set_error(handle, sr);
        return false;
    }

    if (!Player_refresh_env_state(handle->player))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory for environment state");
        return false;
    }

    return true;
}


READ(bind)
{
    (void)indices;
    (void)subkey;

    Bind* map = new_Bind(
            sr, Event_handler_get_names(Player_get_event_handler(handle->player)));
    if (map == NULL)
    {
        set_error(handle, sr);
        return false;
    }

    Module_set_bind(module, map);

    if (!Player_refresh_bind_state(handle->player))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory for bind state");
        return false;
    }

    return true;
}


READ(album_manifest)
{
    (void)indices;
    (void)subkey;

    const bool existent = read_default_manifest(sr);
    if (Streader_is_error_set(sr))
    {
        set_error(handle, sr);
        return false;
    }

    module->album_is_existent = existent;

    return true;
}


READ(album_tracks)
{
    (void)indices;
    (void)subkey;

    Track_list* tl = new_Track_list(sr);
    if (tl == NULL)
    {
        set_error(handle, sr);
        return false;
    }

    del_Track_list(module->track_list);
    module->track_list = tl;

    return true;
}


static Instrument* add_instrument(Handle* handle, int index)
{
    assert(handle != NULL);
    assert(index >= 0);
    assert(index < KQT_INSTRUMENTS_MAX);

    static const char* memory_error_str =
        "Couldn't allocate memory for a new instrument";

    Module* module = Handle_get_module(handle);

    // Return existing instrument
    Instrument* ins = Ins_table_get(Module_get_insts(module), index);
    if (ins != NULL)
        return ins;

    // Create new instrument
    ins = new_Instrument();
    if (ins == NULL || !Ins_table_set(Module_get_insts(module), index, ins))
    {
        Handle_set_error(handle, ERROR_MEMORY, memory_error_str);
        del_Instrument(ins);
        return NULL;
    }

    // Allocate Device state(s) for the new Instrument
    Device_state* ds = Device_create_state(
            (Device*)ins,
            Player_get_audio_rate(handle->player),
            Player_get_audio_buffer_size(handle->player));
    if (ds == NULL || !Device_states_add_state(
                Player_get_device_states(handle->player), ds))
    {
        Handle_set_error(handle, ERROR_MEMORY, memory_error_str);
        Ins_table_remove(Module_get_insts(module), index);
        return NULL;
    }

    return ins;
}


#define acquire_ins_index(index)                           \
    if (true)                                              \
    {                                                      \
        (index) = indices[0];                              \
        if ((index) < 0 || (index) >= KQT_INSTRUMENTS_MAX) \
            return true;                                   \
    }                                                      \
    else (void)0


#define acquire_ins(ins, index)                \
    if (true)                                  \
    {                                          \
        (ins) = add_instrument(handle, index); \
        if ((ins) == NULL)                     \
            return false;                      \
    }                                          \
    else (void)0


static bool is_ins_conn_possible(const Module* module, int32_t ins_index)
{
    assert(module != NULL);
    return (Ins_table_get(Module_get_insts(module), ins_index) != NULL);
}


#define check_update_ins_conns(handle, module, index, was_conn_possible)      \
    if (true)                                                                 \
    {                                                                         \
        const bool changed =                                                  \
            ((was_conn_possible) != is_ins_conn_possible((module), (index))); \
        if (changed && !prepare_connections((handle)))                        \
            return false;                                                     \
    }                                                                         \
    else (void)0


READ(ins_manifest)
{
    (void)subkey;

    int32_t index = -1;
    acquire_ins_index(index);

    const bool was_conn_possible = is_ins_conn_possible(module, index);

    Instrument* ins = NULL;
    acquire_ins(ins, index);

    const bool existent = read_default_manifest(sr);
    if (Streader_is_error_set(sr))
    {
        set_error(handle, sr);
        return false;
    }

    Device_set_existent((Device*)ins, existent);

    check_update_ins_conns(handle, module, index, was_conn_possible);

    return true;
}


READ(ins)
{
    (void)module;
    (void)subkey;

    int32_t index = -1;
    acquire_ins_index(index);

    const bool was_conn_possible = is_ins_conn_possible(module, index);

    Instrument* ins = NULL;
    acquire_ins(ins, index);

    if (!Instrument_parse_header(ins, sr))
    {
        set_error(handle, sr);
        return false;
    }

    check_update_ins_conns(handle, module, index, was_conn_possible);

    return true;
}


READ(ins_connections)
{
    (void)subkey;

    int32_t index = -1;
    acquire_ins_index(index);

    Instrument* ins = NULL;
    acquire_ins(ins, index);

    bool reconnect = false;
    if (!Streader_has_data(sr))
    {
        Instrument_set_connections(ins, NULL);
        reconnect = true;
    }
    else
    {
        Connections* graph = new_Connections_from_string(
                sr,
                CONNECTION_LEVEL_INSTRUMENT,
                Module_get_insts(module),
                Instrument_get_effects(ins),
                NULL,
                (Device*)ins);
        if (graph == NULL)
        {
            set_error(handle, sr);
            return false;
        }

        Instrument_set_connections(ins, graph);
        reconnect = true;
    }

    if (reconnect && !prepare_connections(handle))
        return false;

    return true;
}


READ(ins_env_force)
{
    (void)module;
    (void)subkey;

    int32_t index = -1;
    acquire_ins_index(index);

    const bool was_conn_possible = is_ins_conn_possible(module, index);

    Instrument* ins = NULL;
    acquire_ins(ins, index);

    if (!Instrument_params_parse_env_force(Instrument_get_params(ins), sr))
    {
        set_error(handle, sr);
        return false;
    }

    check_update_ins_conns(handle, module, index, was_conn_possible);

    return true;
}


READ(ins_env_force_release)
{
    (void)module;
    (void)subkey;

    int32_t index = -1;
    acquire_ins_index(index);

    const bool was_conn_possible = is_ins_conn_possible(module, index);

    Instrument* ins = NULL;
    acquire_ins(ins, index);

    if (!Instrument_params_parse_env_force_rel(Instrument_get_params(ins), sr))
    {
        set_error(handle, sr);
        return false;
    }

    check_update_ins_conns(handle, module, index, was_conn_possible);

    return true;
}


READ(ins_env_force_filter)
{
    (void)module;
    (void)subkey;

    int32_t index = -1;
    acquire_ins_index(index);

    const bool was_conn_possible = is_ins_conn_possible(module, index);

    Instrument* ins = NULL;
    acquire_ins(ins, index);

    if (!Instrument_params_parse_env_force_filter(Instrument_get_params(ins), sr))
    {
        set_error(handle, sr);
        return false;
    }

    check_update_ins_conns(handle, module, index, was_conn_possible);

    return true;
}


READ(ins_env_pitch_pan)
{
    (void)module;
    (void)subkey;

    int32_t index = -1;
    acquire_ins_index(index);

    const bool was_conn_possible = is_ins_conn_possible(module, index);

    Instrument* ins = NULL;
    acquire_ins(ins, index);

    if (!Instrument_params_parse_env_pitch_pan(Instrument_get_params(ins), sr))
    {
        set_error(handle, sr);
        return false;
    }

    check_update_ins_conns(handle, module, index, was_conn_possible);

    return true;
}


static Generator* add_generator(
        Handle* handle,
        Instrument* ins,
        Gen_table* gen_table,
        int gen_index)
{
    assert(handle != NULL);
    assert(ins != NULL);
    assert(gen_table != NULL);
    assert(gen_index >= 0);
    assert(gen_index < KQT_GENERATORS_MAX);

    static const char* memory_error_str =
        "Couldn't allocate memory for a new generator";

    // Return existing generator
    Generator* gen = Gen_table_get_gen_mut(gen_table, gen_index);
    if (gen != NULL)
        return gen;

    // Create new generator
    gen = new_Generator(Instrument_get_params(ins));
    if (gen == NULL || !Gen_table_set_gen(gen_table, gen_index, gen))
    {
        Handle_set_error(handle, ERROR_MEMORY, memory_error_str);
        del_Generator(gen);
        return NULL;
    }

    return gen;
}


#define acquire_gen_index(index)                          \
    if (true)                                             \
    {                                                     \
        (index) = indices[1];                             \
        if ((index) < 0 || (index) >= KQT_GENERATORS_MAX) \
            return true;                                  \
    }                                                     \
    else (void)0


static bool is_gen_conn_possible(
        const Module* module, int32_t ins_index, int32_t gen_index)
{
    assert(module != NULL);

    const Instrument* ins = Ins_table_get(Module_get_insts(module), ins_index);
    if (ins == NULL)
        return false;

    const Generator* gen = Instrument_get_gen(ins, gen_index);
    return (gen != NULL) && Device_has_complete_type((const Device*)gen);
}


#define check_update_gen_conns(                                            \
        handle, module, ins_index, gen_index, was_conn_possible)           \
    if (true)                                                              \
    {                                                                      \
        const bool changed = ((was_conn_possible) !=                       \
                is_gen_conn_possible((module), (ins_index), (gen_index))); \
        if (changed && !prepare_connections((handle)))                     \
            return false;                                                  \
    }                                                                      \
    else (void)0


READ(gen_manifest)
{
    (void)module;
    (void)subkey;

    int32_t ins_index = -1;
    acquire_ins_index(ins_index);

    int32_t gen_index = -1;
    acquire_gen_index(gen_index);

    const bool was_conn_possible = is_gen_conn_possible(module, ins_index, gen_index);

    Instrument* ins = NULL;
    acquire_ins(ins, ins_index);

    Gen_table* table = Instrument_get_gens(ins);
    assert(table != NULL);

    const bool existent = read_default_manifest(sr);
    if (Streader_is_error_set(sr))
    {
        set_error(handle, sr);
        return false;
    }

    Gen_table_set_existent(table, gen_index, existent);

    check_update_gen_conns(handle, module, ins_index, gen_index, was_conn_possible);

    return true;
}


READ(gen_type)
{
    (void)subkey;

    int32_t ins_index = -1;
    acquire_ins_index(ins_index);

    int32_t gen_index = -1;
    acquire_gen_index(gen_index);

    const bool was_conn_possible = is_gen_conn_possible(module, ins_index, gen_index);

    if (!Streader_has_data(sr))
    {
        // Remove generator
        Instrument* ins = Ins_table_get(Module_get_insts(module), ins_index);
        if (ins == NULL)
            return true;

        Gen_table* gen_table = Instrument_get_gens(ins);
        Gen_table_remove_gen(gen_table, gen_index);

        check_update_gen_conns(handle, module, ins_index, gen_index, was_conn_possible);

        return true;
    }

    Instrument* ins = NULL;
    acquire_ins(ins, ins_index);
    Gen_table* gen_table = Instrument_get_gens(ins);

    Generator* gen = add_generator(handle, ins, gen_table, gen_index);
    if (gen == NULL)
        return false;

    // Create the Generator implementation
    char type[GEN_TYPE_LENGTH_MAX] = "";
    if (!Streader_read_string(sr, GEN_TYPE_LENGTH_MAX, type))
    {
        set_error(handle, sr);
        return false;
    }

    Generator_cons* cons = Gen_type_find_cons(type);
    if (cons == NULL)
    {
        Handle_set_error(handle, ERROR_FORMAT,
                "Unsupported Generator type: %s", type);
        return false;
    }

    Device_impl* gen_impl = cons(gen);
    if (gen_impl == NULL)
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory for generator implementation");
        return false;
    }

    Device_set_impl((Device*)gen, gen_impl);

    // Remove old Generator Device state
    Device_states* dstates = Player_get_device_states(handle->player);
    Device_states_remove_state(dstates, Device_get_id((Device*)gen));

    // Get generator properties
    Generator_property* property = Gen_type_find_property(type);
    if (property != NULL)
    {
        // Allocate Voice state space
        const char* size_str = property(gen, "voice_state_size");
        if (size_str != NULL)
        {
            Streader* size_sr = Streader_init(
                    STREADER_AUTO, size_str, strlen(size_str));
            int64_t size = 0;
            Streader_read_int(size_sr, &size);
            assert(!Streader_is_error_set(sr));
            assert(size >= 0);
//            fprintf(stderr, "Reserving space for %" PRId64 " bytes\n",
//                            size);
            if (!Player_reserve_voice_state_space(
                        handle->player, size) ||
                    !Player_reserve_voice_state_space(
                        handle->length_counter, size))
            {
                Handle_set_error(handle, ERROR_MEMORY,
                        "Couldn't allocate memory for generator voice states");
                del_Device_impl(gen_impl);
                return false;
            }
        }

        // Allocate channel-specific generator state space
        const char* gen_state_vars = property(gen, "gen_state_vars");
        if (gen_state_vars != NULL)
        {
            Streader* gsv_sr = Streader_init(
                    STREADER_AUTO,
                    gen_state_vars,
                    strlen(gen_state_vars));

            if (!Player_alloc_channel_gen_state_keys(
                        handle->player, gsv_sr))
            {
                set_error(handle, gsv_sr);
                return false;
            }
        }
    }

    // Allocate Device state(s) for this Generator
    Device_state* ds = Device_create_state(
            (Device*)gen,
            Player_get_audio_rate(handle->player),
            Player_get_audio_buffer_size(handle->player));
    if (ds == NULL || !Device_states_add_state(dstates, ds))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory for device state");
        del_Device_state(ds);
        del_Generator(gen);
        return false;
    }

    // Sync the Generator
    if (!Device_sync((Device*)gen))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory while syncing generator");
        return false;
    }

    // Sync the Device state(s)
    if (!Device_sync_states(
                (Device*)gen,
                Player_get_device_states(handle->player)))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory while syncing generator");
        return false;
    }

    check_update_gen_conns(handle, module, ins_index, gen_index, was_conn_possible);

    return true;
}


READ(gen_impl_conf_key)
{
    if (!key_is_device_param(subkey))
        return true;

    (void)module;

    int32_t ins_index = -1;
    acquire_ins_index(ins_index);
    int32_t gen_index = -1;
    acquire_gen_index(gen_index);

    const bool was_conn_possible = is_gen_conn_possible(module, ins_index, gen_index);

    Instrument* ins = NULL;
    acquire_ins(ins, ins_index);
    Gen_table* gen_table = Instrument_get_gens(ins);

    Generator* gen = add_generator(handle, ins, gen_table, gen_index);
    if (gen == NULL)
        return false;

    // Update Device
    if (!Device_set_key((Device*)gen, subkey, sr))
    {
        set_error(handle, sr);
        return false;
    }

    // Update Device state
    Device_set_state_key(
            (Device*)gen,
            Player_get_device_states(handle->player),
            subkey);

    check_update_gen_conns(handle, module, ins_index, gen_index, was_conn_possible);

    return true;
}


READ(gen_impl_key)
{
    assert(strlen(subkey) < KQT_KEY_LENGTH_MAX - 2);

    char hack_subkey[KQT_KEY_LENGTH_MAX] = "i/";
    strcat(hack_subkey, subkey);

    return read_gen_impl_conf_key(handle, module, indices, hack_subkey, sr);
}


READ(gen_conf_key)
{
    assert(strlen(subkey) < KQT_KEY_LENGTH_MAX - 2);

    char hack_subkey[KQT_KEY_LENGTH_MAX] = "c/";
    strcat(hack_subkey, subkey);

    return read_gen_impl_conf_key(handle, module, indices, hack_subkey, sr);
}


static Effect* add_effect(Handle* handle, int index, Effect_table* table)
{
    assert(handle != NULL);
    assert(index >= 0);
    assert(table != NULL);

    static const char* memory_error_str =
        "Couldn't allocate memory for a new effect";

    // Return existing effect
    Effect* eff = Effect_table_get_mut(table, index);
    if (eff != NULL)
        return eff;

    // Create new effect
    eff = new_Effect();
    if (eff == NULL || !Effect_table_set(table, index, eff))
    {
        del_Effect(eff);
        Handle_set_error(handle, ERROR_MEMORY, memory_error_str);
        return NULL;
    }

    // Allocate Device states for the new Effect
    const Device* eff_devices[] =
    {
        (Device*)eff,
        Effect_get_input_interface(eff),
        Effect_get_output_interface(eff),
        NULL
    };
    for (int i = 0; i < 3; ++i)
    {
        assert(eff_devices[i] != NULL);
        Device_state* ds = Device_create_state(
                eff_devices[i],
                Player_get_audio_rate(handle->player),
                Player_get_audio_buffer_size(handle->player));
        if (ds == NULL || !Device_states_add_state(
                    Player_get_device_states(handle->player), ds))
        {
            del_Device_state(ds);
            Handle_set_error(handle, ERROR_MEMORY, memory_error_str);
            Effect_table_remove(table, index);
            return NULL;
        }
    }

    return eff;
}


#define acquire_effect(effect, table, index)             \
    if (true)                                            \
    {                                                    \
        (effect) = add_effect(handle, (index), (table)); \
        if ((effect) == NULL)                            \
            return false;                                \
    }                                                    \
    else (void)0


static int get_effect_index_stop(bool is_instrument)
{
    return (is_instrument ? KQT_INST_EFFECTS_MAX : KQT_EFFECTS_MAX);
}


static int get_effect_index_loc(bool is_instrument)
{
    return (is_instrument ? 1 : 0);
}


static int get_dsp_index_loc(bool is_instrument)
{
    return get_effect_index_loc(is_instrument) + 1;
}


#define acquire_effect_index(index, is_instrument)                     \
    if (true)                                                          \
    {                                                                  \
        const int index_stop = get_effect_index_stop((is_instrument)); \
        (index) = indices[get_effect_index_loc((is_instrument))];      \
        if ((index) < 0 || (index) >= (index_stop))                    \
            return true;                                               \
    }                                                                  \
    else (void)0


static bool is_eff_conn_possible(const Effect_table* eff_table, int32_t eff_index)
{
    assert(eff_table != NULL);
    return (Effect_table_get(eff_table, eff_index) != NULL);
}


#define check_update_eff_conns(                                  \
        handle, eff_table, eff_index, was_conn_possible)         \
    if (true)                                                    \
    {                                                            \
        const bool changed = ((was_conn_possible) !=             \
                is_eff_conn_possible((eff_table), (eff_index))); \
        if (changed && !prepare_connections((handle)))           \
            return false;                                        \
    }                                                            \
    else (void)0


#define READ_EFFECT(name) static bool read_effect_##name( \
        Handle* handle,                                   \
        Module* module,                                   \
        const Key_indices indices,                        \
        const char* subkey,                               \
        Streader* sr,                                     \
        Effect_table* eff_table,                          \
        bool is_instrument)


READ_EFFECT(effect_manifest)
{
    (void)module;
    (void)subkey;

    int32_t eff_index = -1;
    acquire_effect_index(eff_index, is_instrument);

    const bool was_conn_possible = is_eff_conn_possible(eff_table, eff_index);

    Effect* effect = NULL;
    acquire_effect(effect, eff_table, eff_index);

    const bool existent = read_default_manifest(sr);
    if (Streader_is_error_set(sr))
    {
        set_error(handle, sr);
        return false;
    }

    Device_set_existent((Device*)effect, existent);

    check_update_eff_conns(handle, eff_table, eff_index, was_conn_possible);

    return true;
}


READ_EFFECT(effect_connections)
{
    (void)subkey;

    int32_t eff_index = -1;
    acquire_effect_index(eff_index, is_instrument);

    Effect* effect = NULL;
    acquire_effect(effect, eff_table, eff_index);

    bool reconnect = false;
    if (!Streader_has_data(sr))
    {
        Effect_set_connections(effect, NULL);
        reconnect = true;
    }
    else
    {
        Connection_level level = CONNECTION_LEVEL_EFFECT;
        if (eff_table != Module_get_effects(module))
            level |= CONNECTION_LEVEL_INSTRUMENT;

        Connections* graph = new_Connections_from_string(
                sr,
                level,
                Module_get_insts(module),
                eff_table,
                Effect_get_dsps(effect),
                (Device*)effect);
        if (graph == NULL)
        {
            set_error(handle, sr);
            return false;
        }

        Effect_set_connections(effect, graph);
        reconnect = true;
    }

    if (reconnect && !prepare_connections(handle))
        return false;

    return true;
}


static DSP* add_dsp(
        Handle* handle,
        DSP_table* dsp_table,
        int dsp_index)
{
    assert(handle != NULL);
    assert(dsp_table != NULL);
    assert(dsp_index >= 0);
    //assert(dsp_index < KQT_DSPS_MAX);

    static const char* memory_error_str =
        "Couldn't allocate memory for a new DSP";

    // Return existing DSP
    DSP* dsp = DSP_table_get_dsp(dsp_table, dsp_index);
    if (dsp != NULL)
        return dsp;

    // Create new DSP
    dsp = new_DSP();
    if (dsp == NULL || !DSP_table_set_dsp(dsp_table, dsp_index, dsp))
    {
        Handle_set_error(handle, ERROR_MEMORY, memory_error_str);
        del_DSP(dsp);
        return NULL;
    }

    return dsp;
}


#define acquire_dsp_index(index, is_instrument)                \
    if (true)                                                  \
    {                                                          \
        (index) = indices[get_dsp_index_loc((is_instrument))]; \
        if ((index) < 0 || (index) >= KQT_DSPS_MAX)            \
            return true;                                       \
    }                                                          \
    else (void)0


static bool is_dsp_conn_possible(
        const Effect_table* eff_table, int32_t eff_index, int32_t dsp_index)
{
    assert(eff_table != NULL);

    const Effect* eff = Effect_table_get(eff_table, eff_index);
    if (eff == NULL)
        return false;

    const DSP* dsp = Effect_get_dsp(eff, dsp_index);
    if (dsp == NULL)
        return false;

    return Device_has_complete_type((const Device*)dsp);
}


#define check_update_dsp_conns(                                               \
        handle, eff_table, eff_index, dsp_index, was_conn_possible)           \
    if (true)                                                                 \
    {                                                                         \
        const bool changed = ((was_conn_possible) !=                          \
                is_dsp_conn_possible((eff_table), (eff_index), (dsp_index))); \
        if (changed && !prepare_connections((handle)))                        \
            return false;                                                     \
    }                                                                         \
    else (void)0


READ_EFFECT(dsp_manifest)
{
    (void)module;
    (void)subkey;

    int32_t eff_index = -1;
    acquire_effect_index(eff_index, is_instrument);
    int32_t dsp_index = -1;
    acquire_dsp_index(dsp_index, is_instrument);

    const bool was_conn_possible = is_dsp_conn_possible(eff_table, eff_index, dsp_index);

    const bool existent = read_default_manifest(sr);
    if (Streader_is_error_set(sr))
    {
        set_error(handle, sr);
        return false;
    }

    Effect* effect = NULL;
    if (existent)
    {
        acquire_effect(effect, eff_table, eff_index);
    }
    else
    {
        effect = Effect_table_get_mut(eff_table, eff_index);
        if (effect == NULL)
        {
            check_update_dsp_conns(
                    handle, eff_table, eff_index, dsp_index, was_conn_possible);
            return true;
        }
    }

    DSP_table* dsp_table = Effect_get_dsps_mut(effect);
    DSP_table_set_existent(dsp_table, dsp_index, existent);

    check_update_dsp_conns(handle, eff_table, eff_index, dsp_index, was_conn_possible);

    return true;
}


READ_EFFECT(dsp_type)
{
    (void)module;
    (void)subkey;

    int32_t eff_index = -1;
    acquire_effect_index(eff_index, is_instrument);
    int32_t dsp_index = -1;
    acquire_dsp_index(dsp_index, is_instrument);

    const bool was_conn_possible = is_dsp_conn_possible(eff_table, eff_index, dsp_index);

    if (!Streader_has_data(sr))
    {
        // Remove DSP
        Effect* effect = Effect_table_get_mut(eff_table, eff_index);
        if (effect == NULL)
            return true;

        DSP_table* dsp_table = Effect_get_dsps_mut(effect);
        DSP_table_remove_dsp(dsp_table, dsp_index);

        check_update_dsp_conns(
                handle, eff_table, eff_index, dsp_index, was_conn_possible);

        return true;
    }

    Effect* effect = NULL;
    acquire_effect(effect, eff_table, eff_index);
    DSP_table* dsp_table = Effect_get_dsps_mut(effect);

    DSP* dsp = add_dsp(handle, dsp_table, dsp_index);
    if (dsp == NULL)
        return false;

    // Create the DSP implementation
    char type[DSP_TYPE_LENGTH_MAX] = "";
    if (!Streader_read_string(sr, DSP_TYPE_LENGTH_MAX, type))
    {
        set_error(handle, sr);
        return false;
    }
    DSP_cons* cons = DSP_type_find_cons(type);
    if (cons == NULL)
    {
        Handle_set_error(handle, ERROR_FORMAT,
                "Unsupported DSP type: %s", type);
        return false;
    }
    Device_impl* dsp_impl = cons(dsp);
    if (dsp_impl == NULL)
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory for DSP implementation");
        return false;
    }

    Device_set_impl((Device*)dsp, dsp_impl);

    // Remove old DSP Device state
    Device_states* dstates = Player_get_device_states(handle->player);
    Device_states_remove_state(dstates, Device_get_id((Device*)dsp));

    // Allocate Device state(s) for this DSP
    Device_state* ds = Device_create_state(
            (Device*)dsp,
            Player_get_audio_rate(handle->player),
            Player_get_audio_buffer_size(handle->player));
    if (ds == NULL || !Device_states_add_state(
                Player_get_device_states(handle->player), ds))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory for device state");
        del_Device_state(ds);
        del_DSP(dsp);
        return false;
    }

    // Set DSP resources
    if (!Device_set_audio_rate((Device*)dsp,
                dstates,
                Player_get_audio_rate(handle->player)) ||
            !Device_set_buffer_size((Device*)dsp,
                dstates,
                Player_get_audio_buffer_size(handle->player)))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory for DSP state");
        return false;
    }

    // Sync the DSP
    if (!Device_sync((Device*)dsp))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory while syncing DSP");
        return false;
    }

    // Sync the Device state(s)
    if (!Device_sync_states((Device*)dsp, Player_get_device_states(handle->player)))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory while syncing DSP");
        return false;
    }

    check_update_dsp_conns(handle, eff_table, eff_index, dsp_index, was_conn_possible);

    return true;
}


READ_EFFECT(dsp_impl_conf_key)
{
    if (!key_is_device_param(subkey))
        return true;

    (void)module;

    int32_t eff_index = -1;
    acquire_effect_index(eff_index, is_instrument);
    int32_t dsp_index = -1;
    acquire_dsp_index(dsp_index, is_instrument);

    const bool was_conn_possible = is_dsp_conn_possible(eff_table, eff_index, dsp_index);

    Effect* effect = NULL;
    acquire_effect(effect, eff_table, eff_index);
    DSP_table* dsp_table = Effect_get_dsps_mut(effect);

    DSP* dsp = add_dsp(handle, dsp_table, dsp_index);
    if (dsp == NULL)
        return false;

    // Update Device
    if (!Device_set_key((Device*)dsp, subkey, sr))
    {
        set_error(handle, sr);
        return false;
    }

    // Notify Device state
    Device_set_state_key(
            (Device*)dsp,
            Player_get_device_states(handle->player),
            subkey);

    check_update_dsp_conns(handle, eff_table, eff_index, dsp_index, was_conn_possible);

    return true;
}


READ_EFFECT(dsp_impl_key)
{
    assert(strlen(subkey) < KQT_KEY_LENGTH_MAX - 2);

    char hack_subkey[KQT_KEY_LENGTH_MAX] = "i/";
    strcat(hack_subkey, subkey);

    return read_effect_dsp_impl_conf_key(
            handle, module, indices, hack_subkey, sr, eff_table, is_instrument);
}


READ_EFFECT(dsp_conf_key)
{
    assert(strlen(subkey) < KQT_KEY_LENGTH_MAX - 2);

    char hack_subkey[KQT_KEY_LENGTH_MAX] = "c/";
    strcat(hack_subkey, subkey);

    return read_effect_dsp_impl_conf_key(
            handle, module, indices, hack_subkey, sr, eff_table, is_instrument);
}


READ(ins_effect_manifest)
{
    int32_t ins_index = -1;
    acquire_ins_index(ins_index);

    Instrument* ins = NULL;
    acquire_ins(ins, ins_index);

    const bool is_instrument = true;
    return read_effect_effect_manifest(
            handle, module, indices, subkey, sr,
            Instrument_get_effects(ins),
            is_instrument);
}


READ(ins_effect_connections)
{
    int32_t ins_index = -1;
    acquire_ins_index(ins_index);

    Instrument* ins = NULL;
    acquire_ins(ins, ins_index);

    const bool is_instrument = true;
    return read_effect_effect_connections(
            handle, module, indices, subkey, sr,
            Instrument_get_effects(ins),
            is_instrument);
}


READ(ins_dsp_manifest)
{
    int32_t ins_index = -1;
    acquire_ins_index(ins_index);

    Instrument* ins = NULL;
    acquire_ins(ins, ins_index);

    const bool is_instrument = true;
    return read_effect_dsp_manifest(
            handle, module, indices, subkey, sr,
            Instrument_get_effects(ins),
            is_instrument);
}


READ(ins_dsp_type)
{
    int32_t ins_index = -1;
    acquire_ins_index(ins_index);

    Instrument* ins = NULL;
    acquire_ins(ins, ins_index);

    const bool is_instrument = true;
    return read_effect_dsp_type(
            handle, module, indices, subkey, sr,
            Instrument_get_effects(ins),
            is_instrument);
}


READ(ins_dsp_impl_key)
{
    int32_t ins_index = -1;
    acquire_ins_index(ins_index);

    Instrument* ins = NULL;
    acquire_ins(ins, ins_index);

    const bool is_instrument = true;
    return read_effect_dsp_impl_key(
            handle, module, indices, subkey, sr,
            Instrument_get_effects(ins),
            is_instrument);
}


READ(ins_dsp_conf_key)
{
    int32_t ins_index = -1;
    acquire_ins_index(ins_index);

    Instrument* ins = NULL;
    acquire_ins(ins, ins_index);

    const bool is_instrument = true;
    return read_effect_dsp_conf_key(
            handle, module, indices, subkey, sr,
            Instrument_get_effects(ins),
            is_instrument);
}


READ(effect_manifest)
{
    const bool is_instrument = false;
    return read_effect_effect_manifest(
            handle, module, indices, subkey, sr,
            Module_get_effects(module),
            is_instrument);
}


READ(effect_connections)
{
    const bool is_instrument = false;
    return read_effect_effect_connections(
            handle, module, indices, subkey, sr,
            Module_get_effects(module),
            is_instrument);
}


READ(dsp_manifest)
{
    const bool is_instrument = false;
    return read_effect_dsp_manifest(
            handle, module, indices, subkey, sr,
            Module_get_effects(module),
            is_instrument);
}


READ(dsp_type)
{
    const bool is_instrument = false;
    return read_effect_dsp_type(
            handle, module, indices, subkey, sr,
            Module_get_effects(module),
            is_instrument);
}


READ(dsp_impl_key)
{
    const bool is_instrument = false;
    return read_effect_dsp_impl_key(
            handle, module, indices, subkey, sr,
            Module_get_effects(module),
            is_instrument);
}


READ(dsp_conf_key)
{
    const bool is_instrument = false;
    return read_effect_dsp_conf_key(
            handle, module, indices, subkey, sr,
            Module_get_effects(module),
            is_instrument);
}


#define acquire_pattern(pattern, index)                                 \
    if (true)                                                           \
    {                                                                   \
        (pattern) = Pat_table_get(Module_get_pats(module), (index));    \
        if ((pattern) == NULL)                                          \
        {                                                               \
            (pattern) = new_Pattern();                                  \
            if ((pattern) == NULL || !Pat_table_set(                    \
                        Module_get_pats(module), (index), (pattern)))   \
            {                                                           \
                del_Pattern((pattern));                                 \
                Handle_set_error(handle, ERROR_MEMORY,                  \
                        "Could not allocate memory for a new pattern"); \
                return false;                                           \
            }                                                           \
        }                                                               \
    } else (void)0


#define acquire_pattern_index(index)                    \
    if (true)                                           \
    {                                                   \
        (index) = indices[0];                           \
        if ((index) < 0 || (index) >= KQT_PATTERNS_MAX) \
            return true;                                \
    }                                                   \
    else (void)0


READ(pattern_manifest)
{
    (void)subkey;

    int32_t index = -1;
    acquire_pattern_index(index);

    const bool existent = read_default_manifest(sr);
    if (Streader_is_error_set(sr))
    {
        set_error(handle, sr);
        return false;
    }
    Pat_table* pats = Module_get_pats(module);
    Pat_table_set_existent(pats, index, existent);

    return true;
}


READ(pattern)
{
    (void)subkey;

    int32_t index = -1;
    acquire_pattern_index(index);

    Pattern* pattern = NULL;
    acquire_pattern(pattern, index);

    if (!Pattern_parse_header(pattern, sr))
    {
        set_error(handle, sr);
        return false;
    }

    return true;
}


READ(column)
{
    (void)subkey;

    int32_t pat_index = -1;
    acquire_pattern_index(pat_index);

    const int32_t col_index = indices[1];
    if (col_index >= KQT_COLUMNS_MAX)
        return true;

    Pattern* pattern = NULL;
    acquire_pattern(pattern, pat_index);

    const Event_names* event_names =
            Event_handler_get_names(Player_get_event_handler(handle->player));
    Column* column = new_Column_from_string(
            sr, Pattern_get_length(pattern), event_names);
    if (column == NULL)
    {
        set_error(handle, sr);
        return false;
    }
    if (!Pattern_set_column(pattern, col_index, column))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Could not allocate memory for a new column");
        return false;
    }

    return true;
}


READ(pat_instance_manifest)
{
    (void)subkey;

    int32_t pat_index = -1;
    acquire_pattern_index(pat_index);

    const int32_t pinst_index = indices[1];
    if (pinst_index < 0 || pinst_index >= KQT_PAT_INSTANCES_MAX)
        return true;

    Pattern* pattern = NULL;
    acquire_pattern(pattern, pat_index);

    const bool existent = read_default_manifest(sr);
    if (Streader_is_error_set(sr))
    {
        set_error(handle, sr);
        return false;
    }

    Pattern_set_inst_existent(pattern, pinst_index, existent);

    return true;
}


READ(scale)
{
    (void)subkey;

    const int32_t index = indices[0];

    if (index < 0 || index >= KQT_SCALES_MAX)
        return true;

    Scale* scale = new_Scale_from_string(sr);
    if (scale == NULL)
    {
        set_error(handle, sr);
        return false;
    }

    Module_set_scale(module, indices[0], scale);
    return true;
}


#define acquire_song_index(index)                          \
    if (true)                                              \
    {                                                      \
        (index) = indices[0];                              \
        if (indices[0] < 0 || indices[0] >= KQT_SONGS_MAX) \
            return true;                                   \
    }                                                      \
    else (void)0


READ(song_manifest)
{
    (void)subkey;

    int32_t index = 0;
    acquire_song_index(index);

    const bool existent = read_default_manifest(sr);
    if (Streader_is_error_set(sr))
    {
        set_error(handle, sr);
        return false;
    }

    Song_table_set_existent(module->songs, index, existent);

    return true;
}


READ(song)
{
    (void)subkey;

    int32_t index = 0;
    acquire_song_index(index);

    Song* song = new_Song_from_string(sr);
    if (song == NULL)
    {
        set_error(handle, sr);
        return false;
    }

    Song_table* st = Module_get_songs(module);
    if (!Song_table_set(st, index, song))
    {
        Handle_set_error(handle, ERROR_MEMORY,
                "Couldn't allocate memory");
        del_Song(song);
        return false;
    }

    return true;
}


READ(song_order_list)
{
    (void)subkey;

    int32_t index = 0;
    acquire_song_index(index);

    Order_list* ol = new_Order_list(sr);
    if (ol == NULL)
    {
        set_error(handle, sr);
        return false;
    }

    if (module->order_lists[index] != NULL)
        del_Order_list(module->order_lists[index]);

    module->order_lists[index] = ol;

    return true;
}


