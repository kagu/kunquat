

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010-2011
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

#include <Instrument_params.h>
#include <File_base.h>
#include <string_common.h>
#include <xassert.h>


#define new_env_or_fail(env, nodes, xmin, xmax, xstep, ymin, ymax, ystep) \
    if (true)                                                             \
    {                                                                     \
        (env) = new_Envelope((nodes), (xmin), (xmax), (xstep),            \
                (ymin), (ymax), (ystep));                                 \
        if ((env) == NULL)                                                \
        {                                                                 \
            Instrument_params_uninit(ip);                                 \
            return NULL;                                                  \
        }                                                                 \
    } else (void)0

Instrument_params* Instrument_params_init(Instrument_params* ip,
                                          Scale*** scale)
{
    assert(ip != NULL);
    assert(scale != NULL);
    assert(*scale != NULL);
    ip->force_volume_env = NULL;
    ip->env_force_filter = NULL;
    ip->force_pitch_env = NULL;
    ip->env_force = NULL;
    ip->env_force_rel = NULL;
    ip->env_pitch_pan = NULL;
    ip->filter_env = NULL;
    ip->filter_off_env = NULL;
    ip->scale = scale;

    ip->pedal = 0;
    ip->volume = 1;
    ip->force_variation = 0;

    for (int i = 0; i < KQT_GENERATORS_MAX; ++i)
    {
        ip->pitch_locks[i].enabled = false;
        ip->pitch_locks[i].cents = 0;
        ip->pitch_locks[i].freq = exp2(0 / 1200.0) * 440;
    }
#if 0
    ip->pitch_lock_enabled = false;
    ip->pitch_lock_cents = 0;
    ip->pitch_lock_freq = exp2(ip->pitch_lock_cents / 1200.0) * 440;
#endif

    new_env_or_fail(ip->force_volume_env, 8,  0, 1, 0,  0, 1, 0);
    ip->force_volume_env_enabled = false;
    Envelope_set_node(ip->force_volume_env, 0, 0);
    Envelope_set_node(ip->force_volume_env, 1, 1);
    Envelope_set_first_lock(ip->force_volume_env, true, true);
    Envelope_set_last_lock(ip->force_volume_env, true, false);

    new_env_or_fail(ip->env_force_filter, 8,  0, 1, 0,  0, 1, 0);
    ip->env_force_filter_enabled = false;
    Envelope_set_node(ip->env_force_filter, 0, 1);
    Envelope_set_node(ip->env_force_filter, 1, 1);
    Envelope_set_first_lock(ip->env_force_filter, true, false);
    Envelope_set_last_lock(ip->env_force_filter, true, false);

    new_env_or_fail(ip->force_pitch_env, 8,  0, 1, 0,  -1, 1, 0);
    ip->force_pitch_env_enabled = false;
    Envelope_set_node(ip->force_pitch_env, 0, 0);
    Envelope_set_node(ip->force_pitch_env, 1, 0);
    Envelope_set_first_lock(ip->force_pitch_env, true, false);
    Envelope_set_last_lock(ip->force_pitch_env, true, false);

    new_env_or_fail(ip->env_force, 32,  0, INFINITY, 0,  0, 1, 0);
    ip->env_force_enabled = false;
    ip->env_force_carry = false;
    ip->env_force_scale_amount = 0;
    ip->env_force_center = 0;
    Envelope_set_node(ip->env_force, 0, 1);
    Envelope_set_node(ip->env_force, 1, 1);
    Envelope_set_first_lock(ip->env_force, true, false);

    new_env_or_fail(ip->env_force_rel, 32,  0, INFINITY, 0,  0, 1, 0);
    ip->env_force_rel_enabled = false;
    ip->env_force_rel_scale_amount = 0;
    ip->env_force_rel_center = 0;
    Envelope_set_node(ip->env_force_rel, 0, 1);
    Envelope_set_node(ip->env_force_rel, 1, 0);
    Envelope_set_first_lock(ip->env_force_rel, true, false);
    Envelope_set_last_lock(ip->env_force_rel, false, true);

    new_env_or_fail(ip->env_pitch_pan, 8,  -6000, 6000, 0,  -1, 1, 0);
    ip->env_pitch_pan_enabled = false;
    Envelope_set_node(ip->env_pitch_pan, -1, 0);
    Envelope_set_node(ip->env_pitch_pan, 0, 0);
    Envelope_set_node(ip->env_pitch_pan, 1, 0);
    Envelope_set_first_lock(ip->env_pitch_pan, true, false);
    Envelope_set_last_lock(ip->env_pitch_pan, true, false);

    new_env_or_fail(ip->filter_env, 32,  0, INFINITY, 0,  0, 1, 0);
    ip->filter_env_enabled = false;
    ip->filter_env_scale = 1;
    ip->filter_env_center = 440;
    Envelope_set_node(ip->filter_env, 0, 1);
    Envelope_set_node(ip->filter_env, 1, 1);
    Envelope_set_first_lock(ip->filter_env, true, false);

    new_env_or_fail(ip->filter_off_env, 32,  0, INFINITY, 0,  0, 1, 0);
    ip->filter_off_env_enabled = false;
    ip->filter_off_env_scale = 1;
    ip->filter_off_env_center = 440;
    Envelope_set_node(ip->filter_off_env, 0, 1);
    Envelope_set_node(ip->filter_off_env, 1, 1);
    Envelope_set_first_lock(ip->filter_off_env, true, false);

    return ip;
}

#undef new_env_or_fail


void Instrument_params_reset(Instrument_params* ip)
{
    assert(ip != NULL);
    ip->pedal = 0;
    return;
}


bool Instrument_params_parse_env_force_filter(Instrument_params* ip,
                                              char* str,
                                              Read_state* state)
{
    assert(ip != NULL);
    assert(state != NULL);
    if (state->error)
    {
        return false;
    }
    bool enabled = false;
    Envelope* env = new_Envelope(8, 0, 1, 0, 0, 1, 0);
    if (env == NULL)
    {
        return false;
    }
    if (str != NULL)
    {
        str = read_const_char(str, '{', state);
        if (state->error)
        {
            del_Envelope(env);
            return false;
        }
        str = read_const_char(str, '}', state);
        if (state->error)
        {
            Read_state_clear_error(state);
            bool expect_key = true;
            while (expect_key)
            {
                char key[128] = { '\0' };
                str = read_string(str, key, 128, state);
                str = read_const_char(str, ':', state);
                if (state->error)
                {
                    del_Envelope(env);
                    return false;
                }
                if (string_eq(key, "enabled"))
                {
                    str = read_bool(str, &enabled, state);
                }
                else if (string_eq(key, "envelope"))
                {
                    str = Envelope_read(env, str, state);
                }
                else
                {
                    Read_state_set_error(state,
                             "Unrecognised key in force-filter envelope: %s", key);
                    del_Envelope(env);
                    return false;
                }
                if (state->error)
                {
                    del_Envelope(env);
                    return false;
                }
                check_next(str, state, expect_key);
            }
        }
    }
    ip->env_force_filter_enabled = enabled;
    Envelope* old_env = ip->env_force_filter;
    ip->env_force_filter = env;
    del_Envelope(old_env);
    return true;
}


bool Instrument_params_parse_env_pitch_pan(Instrument_params* ip,
                                           char* str,
                                           Read_state* state)
{
    assert(ip != NULL);
    assert(state != NULL);
    if (state->error)
    {
        return false;
    }
    bool enabled = false;
    Envelope* env = new_Envelope(32, -6000, 6000, 0, -1, 1, 0);
    if (env == NULL)
    {
        return false;
    }
    if (str != NULL)
    {
        str = read_const_char(str, '{', state);
        if (state->error)
        {
            del_Envelope(env);
            return false;
        }
        str = read_const_char(str, '}', state);
        if (state->error)
        {
            Read_state_clear_error(state);
            bool expect_key = true;
            while (expect_key)
            {
                char key[128] = { '\0' };
                str = read_string(str, key, 128, state);
                str = read_const_char(str, ':', state);
                if (state->error)
                {
                    del_Envelope(env);
                    return false;
                }
                if (string_eq(key, "enabled"))
                {
                    str = read_bool(str, &enabled, state);
                }
                else if (string_eq(key, "envelope"))
                {
                    str = Envelope_read(env, str, state);
                }
                else
                {
                    Read_state_set_error(state,
                             "Unrecognised key in pitch-pan envelope: %s", key);
                    del_Envelope(env);
                    return false;
                }
                if (state->error)
                {
                    del_Envelope(env);
                    return false;
                }
                check_next(str, state, expect_key);
            }
        }
    }
    ip->env_pitch_pan_enabled = enabled;
    Envelope* old_env = ip->env_pitch_pan;
    ip->env_pitch_pan = env;
    del_Envelope(old_env);
    return true;
}


Envelope* parse_env_time(char* str,
                         Read_state* state,
                         bool* enabled,
                         double* scale_amount,
                         double* scale_center,
                         bool* carry,
                         bool release)
{
    assert(state != NULL);
    assert(enabled != NULL);
    assert(scale_amount != NULL);
    assert(scale_center != NULL);
    if (state->error)
    {
        return NULL;
    }
    Envelope* env = new_Envelope(32, 0, INFINITY, 0, 0, 1, 0);
    if (env == NULL)
    {
        return NULL;
    }
    if (str != NULL)
    {
        str = read_const_char(str, '{', state);
        if (state->error)
        {
            del_Envelope(env);
            return NULL;
        }
        str = read_const_char(str, '}', state);
        if (state->error)
        {
            Read_state_clear_error(state);
            bool expect_key = true;
            while (expect_key)
            {
                char key[128] = { '\0' };
                str = read_string(str, key, 128, state);
                str = read_const_char(str, ':', state);
                if (state->error)
                {
                    del_Envelope(env);
                    return NULL;
                }
                if (string_eq(key, "enabled"))
                {
                    str = read_bool(str, enabled, state);
                }
                else if (string_eq(key, "scale_amount"))
                {
                    str = read_double(str, scale_amount, state);
                }
                else if (string_eq(key, "scale_center"))
                {
                    str = read_double(str, scale_center, state);
                }
                else if (string_eq(key, "envelope"))
                {
                    str = Envelope_read(env, str, state);
                }
                else if (carry != NULL && string_eq(key, "carry"))
                {
                    str = read_bool(str, carry, state);
                }
#if 0
                else if (!release && string_eq(key, "loop"))
                {
                    str = read_bool(str, &loop, state);
                }
                else if (!release && string_eq(key, "loop_start"))
                {
                    str = read_int(str, &loop_start, state);
                }
                else if (!release && string_eq(key, "loop_end"))
                {
                    str = read_int(str, &loop_end, state);
                }
#endif
                else
                {
                    Read_state_set_error(state,
                             "Unrecognised key in the envelope: %s", key);
                    del_Envelope(env);
                    return NULL;
                }
                if (state->error)
                {
                    del_Envelope(env);
                    return NULL;
                }
                check_next(str, state, expect_key);
            }
            str = read_const_char(str, '}', state);
            if (state->error)
            {
                del_Envelope(env);
                return NULL;
            }
        }
    }
    if (Envelope_node_count(env) == 0)
    {
        *enabled = false;
        return env;
    }
    int loop_start = Envelope_get_mark(env, 0);
    int loop_end = Envelope_get_mark(env, 1);
    if (release)
    {
        Envelope_set_mark(env, 0, -1);
        Envelope_set_mark(env, 1, -1);
    }
    else if (loop_start >= 0 || loop_end >= 0)
    {
        if (loop_start == -1)
        {
            loop_start = 0;
        }
        if (loop_end < loop_start)
        {
            loop_end = loop_start;
        }
        Envelope_set_mark(env, 0, loop_start);
        Envelope_set_mark(env, 1, loop_end);
    }
    return env;
}


bool Instrument_params_parse_env_force(Instrument_params* ip,
                                       char* str,
                                       Read_state* state)
{
    assert(ip != NULL);
    assert(state != NULL);
    if (state->error)
    {
        return false;
    }
    bool enabled = false;
    double scale_amount = 0;
    double scale_center = 0;
    bool carry = false;
    Envelope* env = parse_env_time(str, state, &enabled, &scale_amount,
                                   &scale_center, &carry, false);
    if (env == NULL)
    {
        return false;
    }
    assert(!state->error);
    ip->env_force_enabled = enabled;
    ip->env_force_scale_amount = scale_amount;
    ip->env_force_center = exp2(scale_center / 1200) * 440;
    ip->env_force_carry = carry;
    Envelope* old_env = ip->env_force;
    ip->env_force = env;
    del_Envelope(old_env);
    return true;
}


bool Instrument_params_parse_env_force_rel(Instrument_params* ip,
                                           char* str,
                                           Read_state* state)
{
    assert(ip != NULL);
    assert(state != NULL);
    if (state->error)
    {
        return false;
    }
    bool enabled = false;
    double scale_amount = 0;
    double scale_center = 0;
    Envelope* env = parse_env_time(str, state, &enabled, &scale_amount,
                                   &scale_center, NULL, true);
    if (env == NULL)
    {
        return false;
    }
    assert(!state->error);
    ip->env_force_rel_enabled = enabled;
    ip->env_force_rel_scale_amount = scale_amount;
    ip->env_force_rel_center = exp2(scale_center / 1200) * 440;
    Envelope* old_env = ip->env_force_rel;
    ip->env_force_rel = env;
    del_Envelope(old_env);
    return true;
}


#define del_env_check(env)   \
    if (true)                \
    {                        \
        del_Envelope((env)); \
        (env) = NULL;        \
    } else (void)0

void Instrument_params_uninit(Instrument_params* ip)
{
    if (ip == NULL)
    {
        return;
    }
    del_env_check(ip->force_volume_env);
    del_env_check(ip->env_force_filter);
    del_env_check(ip->force_pitch_env);
    del_env_check(ip->env_force);
    del_env_check(ip->env_force_rel);
    del_env_check(ip->env_pitch_pan);
    del_env_check(ip->filter_env);
    del_env_check(ip->filter_off_env);
    return;
}

#undef del_env_check


