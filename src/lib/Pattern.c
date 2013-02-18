

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010-2013
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

#include <Bit_array.h>
#include <Connections_search.h>
#include <Pattern.h>
#include <Pattern_location.h>
#include <Playdata.h>
#include <Event.h>
#include <Event_handler.h>
#include <events/Event_global_jump.h>
#include <xassert.h>
#include <xmemory.h>


struct Pattern
{
    Column* global;
    Column* aux;
    Column* cols[KQT_COLUMNS_MAX];
    AAtree* locations;
    AAiter* locations_iter;
    Reltime length;
    Bit_array* existents;
};


static void evaluate_row(Pattern* pat,
                         Playdata* play,
                         Event_handler* eh,
                         Event** next,
                         Reltime** next_pos);


Pattern* new_Pattern(void)
{
    Pattern* pat = xalloc(Pattern);
    if (pat == NULL)
    {
        return NULL;
    }

    pat->global = NULL;
    pat->aux = NULL;
    for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
        pat->cols[i] = NULL;
    pat->locations = NULL;
    pat->locations_iter = NULL;
    pat->existents = NULL;

    pat->global = new_Column(NULL);
    if (pat->global == NULL)
    {
        del_Pattern(pat);
        return NULL;
    }
    for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
    {
        pat->cols[i] = new_Column(NULL);
        if (pat->cols[i] == NULL)
        {
            del_Pattern(pat);
            return NULL;
        }
    }
    pat->aux = new_Column_aux(NULL, pat->cols[0], 0);
    pat->locations = new_AAtree(
            (int (*)(const void*, const void*))Pattern_location_cmp, free);
    pat->locations_iter = new_AAiter(pat->locations);
    pat->existents = new_Bit_array(KQT_PAT_INSTANCES_MAX);
    if (pat->aux == NULL ||
            pat->locations == NULL ||
            pat->locations_iter == NULL ||
            pat->existents == NULL)
    {
        del_Pattern(pat);
        return NULL;
    }
    Reltime_set(&pat->length, 16, 0);
    return pat;
}


bool Pattern_parse_header(Pattern* pat, char* str, Read_state* state)
{
    assert(pat != NULL);
    assert(state != NULL);
    if (state->error)
    {
        return false;
    }
    Reltime* len = PATTERN_DEFAULT_LENGTH;
    if (str != NULL)
    {
        str = read_const_char(str, '{', state);
        str = read_const_string(str, "length", state);
        str = read_const_char(str, ':', state);
        str = read_reltime(str, len, state);
        str = read_const_char(str, '}', state);
        if (state->error)
        {
            return false;
        }
    }
    if (Reltime_get_beats(len) < 0)
    {
        Read_state_set_error(state, "Pattern length is negative");
        return false;
    }
    Pattern_set_length(pat, len);
    return true;
}


void Pattern_set_inst_existent(Pattern* pat, int index, bool existent)
{
    assert(pat != NULL);
    assert(index >= 0);
    assert(index < KQT_PAT_INSTANCES_MAX);

    Bit_array_set(pat->existents, index, existent);

    return;
}


bool Pattern_get_inst_existent(Pattern* pat, int index)
{
    assert(pat != NULL);
    assert(index >= 0);
    assert(index < KQT_PAT_INSTANCES_MAX);

    return Bit_array_get(pat->existents, index);
}


bool Pattern_set_location(Pattern* pat, int song, Pat_inst_ref* piref)
{
    assert(pat != NULL);
    assert(song >= 0);
    assert(song < KQT_SONGS_MAX);
    assert(piref != NULL);
    Pattern_location* key = PATTERN_LOCATION_AUTO;
    key->song = song;
    key->piref = *piref;
    if (AAtree_get_exact(pat->locations, key) != NULL)
    {
        return true;
    }
    key = new_Pattern_location(song, piref);
    if (key == NULL || !AAtree_ins(pat->locations, key))
    {
        xfree(key);
        return false;
    }
    for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
    {
        if (!Column_update_locations(pat->cols[i],
                                     pat->locations, pat->locations_iter))
        {
            return false;
        }
    }
    return true;
}


AAtree* Pattern_get_locations(Pattern* pat, AAiter** iter)
{
    assert(pat != NULL);
    assert(iter != NULL);
    *iter = pat->locations_iter;
    return pat->locations;
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


bool Pattern_set_col(Pattern* pat, int index, Column* col)
{
    assert(pat != NULL);
    assert(index >= 0);
    assert(index < KQT_COLUMNS_MAX);
    assert(col != NULL);
    Column* new_aux = new_Column_aux(pat->aux, col, index);
    if (new_aux == NULL)
    {
        return false;
    }
    Column* old_aux = pat->aux;
    pat->aux = new_aux;
    del_Column(old_aux);
    Column* old_col = pat->cols[index];
    pat->cols[index] = col;
    del_Column(old_col);
    return true;
}


Column* Pattern_get_col(Pattern* pat, int index)
{
    assert(pat != NULL);
    assert(index >= 0);
    assert(index < KQT_COLUMNS_MAX);
    return pat->cols[index];
}


void Pattern_set_global(Pattern* pat, Column* col)
{
    assert(pat != NULL);
    assert(col != NULL);
    Column* old_col = pat->global;
    pat->global = col;
    del_Column(old_col);
    return;
}


Column* Pattern_get_global(Pattern* pat)
{
    assert(pat != NULL);
    return pat->global;
}


uint32_t Pattern_mix(Pattern* pat,
                     uint32_t nframes,
                     uint32_t offset,
                     Event_handler* eh,
                     Channel** channels,
                     Connections* connections)
{
//    assert(pat != NULL);
    assert(offset < nframes);
    assert(eh != NULL);
    assert(channels != NULL);
    Playdata* play = Event_handler_get_global_state(eh);
    assert(pat != NULL || play->parent.pause);
    assert(play->tempo > 0);
    uint32_t mixed = offset;
//    fprintf(stderr, "new mixing cycle from %" PRIu32 " to %" PRIu32 "\n", offset, nframes);
    const Reltime* zero_time = Reltime_set(RELTIME_AUTO, 0, 0);
    if (pat != NULL && play->mode == PLAY_PATTERN &&
            Reltime_cmp(zero_time, &pat->length) == 0)
    {
        play->mode = STOP;
        return 0;
    }
    if (pat != NULL && Reltime_cmp(&play->pos, &pat->length) > 0)
    {
        Reltime_set(&play->pos, 0, 0);
        if (play->mode == PLAY_PATTERN)
        {
            return 0;
        }
        ++play->system;

        play->piref.pat = -1;
        Track_list* tl = play->track_list;
        if (tl != NULL && play->track < Track_list_get_len(tl))
        {
            const int16_t song_index = Track_list_get_song_index(
                    tl, play->track);

            const bool existent = Subsong_table_get_existent(
                    play->subsongs,
                    song_index);
            assert(play->order_lists != NULL);
            const Order_list* ol = play->order_lists[song_index];
            if (existent && ol != NULL && play->system < Order_list_get_len(ol))
            {
                Pat_inst_ref* ref = Order_list_get_pat_inst_ref(ol, play->system);
                assert(ref != NULL);
                play->piref = *ref;
            }
        }
#if 0
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
#endif
        return 0;
    }
    while (mixed < nframes
            // TODO: and we still want to mix this pattern
            && (pat == NULL || Reltime_cmp(&play->pos, &pat->length) <= 0))
    {
        Event* next = NULL;
        if (pat != NULL && !play->parent.pause)
        {
            Column_iter_change_col(play->citer, pat->aux);
            next = Column_iter_get(play->citer, &play->pos);
        }
        Reltime* next_pos = NULL;
        if (next != NULL)
        {
            next_pos = Event_get_pos(next);
        }
        if (pat != NULL)
        {
            evaluate_row(pat, play, eh, &next, &next_pos);
        }
        if (play->old_tempo != play->tempo || play->old_freq != play->freq)
        {
            Slider_set_mix_rate(&play->volume_slider, play->freq);
            Slider_set_tempo(&play->volume_slider, play->tempo);
            play->old_freq = play->freq;
            play->old_tempo = play->tempo;
        }
        bool delay = Reltime_cmp(&play->delay_left, zero_time) > 0;
        assert(!(delay && (play->jump || play->goto_trigger)));
        if (!delay && !play->parent.pause &&
                (play->jump || play->goto_trigger))
        {
            int16_t* target_subsong = NULL;
            int16_t* target_section = NULL;
            Reltime* target_row = NULL;
            if (play->jump)
            {
                play->jump = false;
                target_subsong = &play->jump_subsong;
                target_section = &play->jump_section;
                target_row = &play->jump_row;
            }
            else
            {
                assert(play->goto_trigger);
                play->goto_trigger = false;
                target_subsong = &play->goto_subsong;
                target_section = &play->goto_section;
                target_row = &play->goto_row;
            }
            assert(target_subsong != NULL);
            assert(target_section != NULL);
            assert(target_row != NULL);
            if (play->mode == PLAY_PATTERN)
            {
                if (*target_subsong < 0 && *target_section < 0)
                {
                    Reltime_copy(&play->pos, target_row);
                }
                else
                {
                    Reltime_set(&play->pos, 0, 0);
                }
                break;
            }
            if (*target_subsong >= 0)
            {
                play->track = *target_subsong;
            }
            if (*target_section >= 0)
            {
                play->system = *target_section; // TODO: remove
            }
            Reltime_copy(&play->pos, target_row);
            break;
        }
        if (!delay && !play->parent.pause && pat != NULL &&
                Reltime_cmp(&play->pos, &pat->length) >= 0)
        {
            assert(Reltime_cmp(&play->pos, &pat->length) == 0);
            Reltime_init(&play->pos);
            if (play->mode == PLAY_PATTERN)
            {
                Reltime_set(&play->pos, 0, 0);
                break;
            }
            ++play->system;

            play->piref.pat = -1;
            const Track_list* tl = play->track_list;
            if (tl != NULL && play->track < Track_list_get_len(tl))
            {
                const int16_t song_index = Track_list_get_song_index(
                        tl, play->track);

                const bool existent = Subsong_table_get_existent(
                        play->subsongs,
                        song_index);
                assert(play->order_lists != NULL);
                const Order_list* ol = play->order_lists[song_index];
                if (existent && ol != NULL && play->system < Order_list_get_len(ol))
                {
                    Pat_inst_ref* ref = Order_list_get_pat_inst_ref(ol, play->system);
                    assert(ref != NULL);
                    play->piref = *ref;
                }
            }
#if 0
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
#endif
            break;
        }
        assert(next == NULL || next_pos != NULL);
        uint32_t to_be_mixed = nframes - mixed;
        if (play->tempo_slide != 0)
        {
            if (Reltime_cmp(&play->tempo_slide_left, zero_time) <= 0)
            {
                play->tempo = play->tempo_slide_target;
                play->tempo_slide = 0;
            }
            else if (Reltime_cmp(&play->tempo_slide_int_left, zero_time) <= 0)
            {
                play->tempo += play->tempo_slide_update;
                if ((play->tempo_slide < 0 && play->tempo < play->tempo_slide_target)
                        || (play->tempo_slide > 0 && play->tempo > play->tempo_slide_target))
                {
                    play->tempo = play->tempo_slide_target;
                    play->tempo_slide = 0;
                }
                else
                {
                    Reltime_set(&play->tempo_slide_int_left, 0, 36756720);
                    if (Reltime_cmp(&play->tempo_slide_int_left, &play->tempo_slide_left) > 0)
                    {
                        Reltime_copy(&play->tempo_slide_int_left, &play->tempo_slide_left);
                    }
                }
            }
        }

        Reltime* limit = Reltime_fromframes(RELTIME_AUTO,
                                            to_be_mixed,
                                            play->tempo,
                                            play->freq);
        if (delay && Reltime_cmp(limit, &play->delay_left) > 0)
        {
            Reltime_copy(limit, &play->delay_left);
            to_be_mixed = Reltime_toframes(limit, play->tempo, play->freq);
        }
        if (play->tempo_slide != 0 && Reltime_cmp(limit, &play->tempo_slide_int_left) > 0)
        {
            Reltime_copy(limit, &play->tempo_slide_int_left);
            to_be_mixed = Reltime_toframes(limit, play->tempo, play->freq);
        }
        Reltime_add(limit, limit, &play->pos);
        // - Check for the end of pattern
        if (!delay && !play->parent.pause && pat != NULL &&
                Reltime_cmp(&pat->length, limit) < 0)
        {
            Reltime_copy(limit, &pat->length);
            to_be_mixed = Reltime_toframes(Reltime_sub(RELTIME_AUTO, limit, &play->pos),
                                           play->tempo,
                                           play->freq);
        }
        // - Check first upcoming event position to figure out
        //   how much we can mix for now
        if (!delay && next != NULL && Reltime_cmp(next_pos, limit) < 0)
        {
            Reltime_copy(limit, next_pos);
            to_be_mixed = Reltime_toframes(Reltime_sub(RELTIME_AUTO,
                                                       limit, &play->pos),
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
            // - Mix the Voices
/*            uint16_t active_voices = Voice_pool_mix_bg(play->voice_pool,
                    to_be_mixed + mixed, mixed, play->freq, play->tempo); */
            uint32_t mix_until = to_be_mixed + mixed;
            for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
            {
                if (pat != NULL)
                {
                    Column_iter_change_col(play->citer, pat->cols[i]);
                }
                Channel_mix(channels[i], play->voice_pool,
                            mix_until, mixed,
                            play->tempo, play->freq);
            }
            uint16_t active_voices = Voice_pool_mix_bg(play->voice_pool,
                    mix_until, mixed, play->freq, play->tempo);
            if (play->active_voices < active_voices)
            {
                play->active_voices = active_voices;
            }
#if 0
            for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
            {
                Channel_update_state(channels[i], mix_until); // FIXME
            }
#endif
            if (connections != NULL)
            {
                Connections_mix(connections, mixed, mix_until,
                                play->freq, play->tempo);
            }
        }
        if (play->volume != 1 || Slider_in_progress(&play->volume_slider))
        {
            Audio_buffer* buffer = NULL;
            if (connections != NULL)
            {
                Device* master = Device_node_get_device(
                                         Connections_get_master(connections));
                buffer = Device_get_buffer(master,
                                 DEVICE_PORT_TYPE_RECEIVE, 0);
            }
            if (!play->silent && buffer != NULL)
            {
                kqt_frame* bufs[] =
                {
                    Audio_buffer_get_buffer(buffer, 0),
                    Audio_buffer_get_buffer(buffer, 1),
                };
                for (uint32_t i = mixed; i < mixed + to_be_mixed; ++i)
                {
                    if (Slider_in_progress(&play->volume_slider))
                    {
                        play->volume = Slider_step(&play->volume_slider);
                    }
                    for (int k = 0; k < KQT_BUFFERS_MAX; ++k)
                    {
                        assert(bufs[k] != NULL);
                        bufs[k][i] *= play->volume;
                    }
                }
            }
            else if (Slider_in_progress(&play->volume_slider))
            {
                Slider_skip(&play->volume_slider, to_be_mixed);
            }
        }
        // - Increment play->pos
        Reltime* adv = Reltime_sub(RELTIME_AUTO, limit, &play->pos);
        if (play->tempo_slide != 0)
        {
            Reltime_sub(&play->tempo_slide_int_left, &play->tempo_slide_int_left, adv);
            Reltime_sub(&play->tempo_slide_left, &play->tempo_slide_left, adv);
        }
        Reltime_add(&play->play_time, &play->play_time, adv);
        if (Reltime_cmp(&play->delay_left, Reltime_init(RELTIME_AUTO)) > 0)
        {
            Reltime_sub(&play->delay_left, &play->delay_left, adv);
        }
        else if (!play->parent.pause)
        {
            Reltime_copy(&play->pos, limit);
        }
        mixed += to_be_mixed;
    }
    return mixed - offset;
}


static void evaluate_row(Pattern* pat,
                         Playdata* play,
                         Event_handler* eh,
                         Event** next,
                         Reltime** next_pos)
{
    assert(pat != NULL);
    assert(play != NULL);
    assert(next != NULL);
    assert(next_pos != NULL);
    (void)pat;
    const Reltime* zero_time = Reltime_set(RELTIME_AUTO, 0, 0);
    play->event_index = 0;
    while (*next != NULL
            && Reltime_cmp(*next_pos, &play->pos) == 0
            && Reltime_cmp(&play->delay_left, zero_time) <= 0
            && !(play->jump || play->goto_trigger))
    {
        Event_type type = Event_get_type(*next);
        if (play->delay_event_index >= 0)
        {
            for (int i = 0; i <= play->delay_event_index; ++i)
            {
                *next = Column_iter_get_next(play->citer);
                ++play->event_index;
            }
            if (*next != NULL)
            {
                *next_pos = Event_get_pos(*next);
            }
            play->delay_event_index = -1;
            if (*next == NULL || Reltime_cmp(*next_pos, &play->pos) != 0)
            {
                break;
            }
        }
        if (Event_get_type(*next) == EVENT_GLOBAL_JUMP)
        {
            if (General_state_events_enabled((General_state*)play))
            {
                // Jump events inside Patterns contain mutable state data, so
                // they need to be handled as a special case here.
                Trigger_global_jump_process(*next, play);
            }
        }
        else if ((!EVENT_IS_CONTROL(type) || play->infinite) &&
                 (!play->silent || EVENT_IS_GLOBAL(type) ||
                                   EVENT_IS_GENERAL(type)))
        {
            //Event_pg* pg = (Event_pg*)*next;
            Event_handler_trigger(eh, (*next)->ch_index, Event_get_desc(*next),
                                  play->silent, NULL);
        }
        ++play->event_index;
        *next = Column_iter_get_next(play->citer);
        if (*next != NULL)
        {
            *next_pos = Event_get_pos(*next);
        }
    }
    return;
}


void del_Pattern(Pattern* pat)
{
    if (pat == NULL)
    {
        return;
    }
    for (int i = 0; i < KQT_COLUMNS_MAX; ++i)
    {
        del_Column(pat->cols[i]);
    }
    del_Column(pat->global);
    del_Column(pat->aux);
    del_AAtree(pat->locations);
    del_AAiter(pat->locations_iter);
    xfree(pat);
    return;
}


