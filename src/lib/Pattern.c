

/*
 * Copyright 2009 Tomi Jylhä-Ollila
 *
 * This file is part of Kunquat.
 *
 * Kunquat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kunquat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kunquat.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#include <Pattern.h>
#include <Playdata.h>
#include <Event_queue.h>
#include <Event.h>
#include <Event_global.h>
#include <Event_ins.h>

#include <xmemory.h>


Pattern* new_Pattern(void)
{
    Pattern* pat = xalloc(Pattern);
    if (pat == NULL)
    {
        return NULL;
    }
    pat->global = new_Column(NULL);
    if (pat->global == NULL)
    {
        xfree(pat);
        return NULL;
    }
    for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
    {
        pat->cols[i] = new_Column(NULL);
        if (pat->cols[i] == NULL)
        {
            for (--i; i >= 0; --i)
            {
                del_Column(pat->cols[i]);
            }
            del_Column(pat->global);
            xfree(pat);
            return NULL;
        }
    }
    Reltime_set(&pat->length, 16, 0);
    return pat;
}


bool Pattern_read(Pattern* pat, File_tree* tree, Read_state* state)
{
    assert(pat != NULL);
    assert(tree != NULL);
    assert(state != NULL);
    if (state->error)
    {
        return false;
    }
    Read_state_init(state, File_tree_get_path(tree));
    if (!File_tree_is_dir(tree))
    {
        Read_state_set_error(state, "Pattern is not a directory");
        return false;
    }
    File_tree* info = File_tree_get_child(tree, "pattern.json");
    if (info != NULL)
    {
        Read_state_init(state, File_tree_get_path(info));
        if (File_tree_is_dir(info))
        {
            Read_state_set_error(state, "Pattern info is a directory");
            return false;
        }
        char* str = File_tree_get_data(info);
        assert(str != NULL);
        str = read_const_char(str, '{', state);
        str = read_const_string(str, "length", state);
        str = read_const_char(str, ':', state);
        Reltime* len = Reltime_init(RELTIME_AUTO);
        str = read_reltime(str, len, state);
        str = read_const_char(str, '}', state);
        if (state->error)
        {
            return false;
        }
        if (Reltime_get_beats(len) < 0)
        {
            Read_state_set_error(state, "Pattern length is negative");
            return false;
        }
        Pattern_set_length(pat, len);
    }
    char dir_name[16] = "global_column";
    for (int i = -1; i < KQT_COLUMNS_MAX; ++i)
    {
        File_tree* col_tree = File_tree_get_child(tree, dir_name);
        if (col_tree != NULL)
        {
            Read_state_init(state, File_tree_get_path(col_tree));
            if (i == -1)
            {
                Column_read(Pattern_get_global(pat), col_tree, state);
            }
            else
            {
                Column_read(Pattern_get_col(pat, i), col_tree, state);
            }
            if (state->error)
            {
                return false;
            }
        }
        snprintf(dir_name, 16, "voice_column_%02x", i + 1);
    }
    return true;
}


void Pattern_set_length(Pattern* pat, Reltime* length)
{
    assert(pat != NULL);
    assert(length != NULL);
    assert(length->beats >= 0);
    Reltime_copy(&pat->length, length);
    return;
}


Reltime* Pattern_get_length(Pattern* pat)
{
    assert(pat != NULL);
    return &pat->length;
}


Column* Pattern_get_col(Pattern* pat, int index)
{
    assert(pat != NULL);
    assert(index >= 0);
    assert(index < KQT_COLUMNS_MAX);
    return pat->cols[index];
}


Column* Pattern_get_global(Pattern* pat)
{
    assert(pat != NULL);
    return pat->global;
}


uint32_t Pattern_mix(Pattern* pat,
                     uint32_t nframes,
                     uint32_t offset,
                     Playdata* play)
{
//  assert(pat != NULL);
    assert(offset < nframes);
    assert(play != NULL);
    uint32_t mixed = offset;
    if (pat == NULL)
    {
        assert(!play->silent);
        Reltime* limit = Reltime_fromframes(RELTIME_AUTO,
                nframes - mixed,
                play->tempo,
                play->freq);
        for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
        {
            Channel_set_voices(play->channels[i],
                    play->voice_pool,
                    NULL,
                    &play->pos,
                    limit,
                    mixed,
                    play->tempo,
                    play->freq);
        }
        uint16_t active_voices = Voice_pool_mix(play->voice_pool,
                nframes, mixed, play->freq, play->tempo);
        if (active_voices == 0)
        {
            play->active_voices = 0;
            play->mode = STOP;
            return nframes;
        }
        if (play->active_voices < active_voices)
        {
            play->active_voices = active_voices;
        }
        return nframes;
    }
    while (mixed < nframes
            // TODO: and we still want to mix this pattern
            && Reltime_cmp(&play->pos, &pat->length) <= 0)
    {
        Column_iter_change_col(play->citer, pat->global);
        Event* next_global = Column_iter_get(play->citer, &play->pos);
        Reltime* next_global_pos = NULL;
        if (next_global != NULL)
        {
            next_global_pos = Event_get_pos(next_global);
        }
        // - Evaluate global events
        while (next_global != NULL
                && Reltime_cmp(next_global_pos, &play->pos) == 0)
        {
            // FIXME: conditional event handling must be processed here
            //        instead of Song_mix.
            if (Event_get_type(next_global) == EVENT_GLOBAL_SET_TEMPO)
            {
                Event_global_process((Event_global*)next_global, play);
            }
            else if (EVENT_IS_GENERAL(Event_get_type(next_global))
                    || EVENT_IS_GLOBAL(Event_get_type(next_global)))
            {
                if (!Event_queue_ins(play->events, next_global, mixed))
                {
                    // Queue is full, ignore remaining events... TODO: notify
                    next_global = Column_iter_get(play->citer,
                            Reltime_add(RELTIME_AUTO, &play->pos,
                                    Reltime_set(RELTIME_AUTO, 0, 1)));
                    if (next_global != NULL)
                    {
                        next_global_pos = Event_get_pos(next_global);
                    }
                    break;
                }
            }
            next_global = Column_iter_get_next(play->citer);
            if (next_global != NULL)
            {
                next_global_pos = Event_get_pos(next_global);
            }
        }
        if (Reltime_cmp(&play->pos, &pat->length) >= 0)
        {
            assert(Reltime_cmp(&play->pos, &pat->length) == 0);
            Reltime_init(&play->pos);
            if (play->mode == PLAY_PATTERN)
            {
                Reltime_set(&play->pos, 0, 0);
                break;
            }
            ++play->section;
            if (play->section >= KQT_SECTIONS_MAX)
            {
                play->section = 0;
                play->pattern = -1;
            }
            else
            {
                play->pattern = KQT_SECTION_NONE;
                Subsong* ss = Subsong_table_get(play->subsongs, play->subsong);
                if (ss != NULL)
                {
                    play->pattern = Subsong_get(ss, play->section);
                }
            }
            break;
        }
        assert(next_global == NULL || next_global_pos != NULL);
        uint32_t to_be_mixed = nframes - mixed;
        Reltime* limit = Reltime_fromframes(RELTIME_AUTO,
                to_be_mixed,
                play->tempo,
                play->freq);
        Reltime_add(limit, limit, &play->pos);
        // - Check for the end of pattern
        if (Reltime_cmp(&pat->length, limit) < 0)
        {
            Reltime_copy(limit, &pat->length);
            to_be_mixed = Reltime_toframes(
                    Reltime_sub(RELTIME_AUTO, limit, &play->pos),
                    play->tempo,
                    play->freq);
        }
        // - Check first upcoming global event position to figure out how much we can mix for now
        if (next_global != NULL && Reltime_cmp(next_global_pos, limit) < 0)
        {
            assert(next_global_pos != NULL);
            Reltime_copy(limit, next_global_pos);
            to_be_mixed = Reltime_toframes(
                    Reltime_sub(RELTIME_AUTO, limit, &play->pos),
                    play->tempo,
                    play->freq);
        }
        // - Calculate the number of frames to be mixed
        assert(Reltime_cmp(&play->pos, limit) <= 0);
        if (to_be_mixed > nframes - mixed)
        {
            to_be_mixed = nframes - mixed;
        }
        if (!play->silent)
        {
            // - Tell each channel to set up Voices
            for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
            {
                Column_iter_change_col(play->citer, pat->cols[i]);
                Channel_set_voices(play->channels[i],
                                   play->voice_pool,
                                   play->citer,
                                   &play->pos,
                                   limit,
                                   mixed,
                                   play->tempo,
                                   play->freq);
            }
            // - Mix the Voice pool
/*            uint16_t active_voices = Voice_pool_mix(play->voice_pool,
                    to_be_mixed + mixed, mixed, play->freq, play->tempo); */
            Event* ins_event = NULL;
            uint32_t ins_event_pos = UINT32_MAX;
            uint32_t mix_until = to_be_mixed + mixed;
            uint32_t pool_mixed = mixed;
            bool event_found = Event_queue_get(play->ins_events, &ins_event, &ins_event_pos);
            if (event_found && ins_event_pos < mix_until)
            {
                mix_until = ins_event_pos;
            }
            while (pool_mixed < to_be_mixed + mixed)
            {
                uint16_t active_voices = Voice_pool_mix(play->voice_pool,
                        mix_until, pool_mixed, play->freq, play->tempo);
                if (play->active_voices < active_voices)
                {
                    play->active_voices = active_voices;
                }
                if (event_found)
                {
                    assert(ins_event != NULL);
                    int64_t* ins_index = Event_get_field(ins_event, 0);
                    assert(ins_index != NULL);
                    Instrument* ins = Ins_table_get(play->channels[0]->insts, *ins_index);
                    if (ins != NULL)
                    {
                        Event_ins_process((Event_ins*)ins_event, &ins->params);
                    }
                }
                pool_mixed = mix_until;
                mix_until = to_be_mixed + mixed;
                event_found = Event_queue_get(play->ins_events, &ins_event, &ins_event_pos);
                if (event_found && ins_event_pos < mix_until)
                {
                    mix_until = ins_event_pos;
                }
            }
            while (event_found)
            {
                assert(ins_event != NULL);
                int64_t* ins_index = Event_get_field(ins_event, 0);
                assert(ins_index != NULL);
                Instrument* ins = Ins_table_get(play->channels[0]->insts, *ins_index);
                if (ins != NULL)
                {
                    Event_ins_process((Event_ins*)ins_event, &ins->params);
                }
                event_found = Event_queue_get(play->ins_events, &ins_event, &ins_event_pos);
            }
            for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
            {
                Channel_update_state(play->channels[i], to_be_mixed + mixed);
            }
        }
        // - Increment play->pos
        Reltime_copy(&play->pos, limit);
        mixed += to_be_mixed;
    }
    return mixed - offset;
}


void del_Pattern(Pattern* pat)
{
    assert(pat != NULL);
    for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
    {
        del_Column(pat->cols[i]);
    }
    del_Column(pat->global);
    xfree(pat);
    return;
}


