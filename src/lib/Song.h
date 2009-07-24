

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


#ifndef K_SONG_H
#define K_SONG_H


#include <stdint.h>

#include <kunquat/limits.h>
#include <kunquat/frame.h>
#include <Subsong_table.h>
#include <Pat_table.h>
#include <Ins_table.h>
#include <Scale.h>
#include <Playdata.h>
#include <File_base.h>
#include <File_tree.h>


typedef struct Song
{
    int buf_count;                      ///< Number of buffers used for mixing.
    uint32_t buf_size;                  ///< Buffer size.
    kqt_frame* bufs[KQT_BUFFERS_MAX];   ///< Buffers.
    kqt_frame* priv_bufs[KQT_BUFFERS_MAX];  ///< Private buffers.
    kqt_frame* voice_bufs[KQT_BUFFERS_MAX]; ///< Temporary buffers for Voices.
    Subsong_table* subsongs;            ///< The Subsongs.
    Pat_table* pats;                    ///< The Patterns.
    Ins_table* insts;                   ///< The Instruments.
    Scale* scales[KQT_SCALES_MAX];      ///< The Scales.
    Scale** active_scale;               ///< A reference to the currently active Scale.
    Event_queue* events;                ///< Global events.
    double mix_vol_dB;                  ///< Mixing volume in dB.
    double mix_vol;                     ///< Mixing volume.
    uint16_t init_subsong;              ///< Initial subsong number.
} Song;


/**
 * Creates a new Song.
 * The caller shall eventually call del_Song() to destroy the Song returned.
 *
 * \param buf_count   Number of buffers to allocate -- must be >= \c 1 and
 *                    <= \a KQT_BUFFERS_MAX. Typically, this is 2 (stereo).
 * \param buf_size    Size of a buffer -- must be > \c 0.
 * \param events      The maximum number of global events per tick -- must be
 *                    > \c 0.
 *
 * \see del_Song()
 *
 * \return   The new Song if successful, or \c NULL if memory allocation
 *           failed.
 */
Song* new_Song(int buf_count, uint32_t buf_size, uint8_t events);


/**
 * Reads a Song from a File tree.
 *
 * \param song    The Song -- must not be \c NULL.
 * \param tree    The File tree -- must not be \c NULL.
 * \param state   The Read state -- must not be \c NULL.
 *
 * \return   \c true if successful, otherwise \c false.
 */
bool Song_read(Song* song, File_tree* tree, Read_state* state);


/**
 * Mixes a portion of the Song.
 *
 * \param song      The Song -- must not be \c NULL.
 * \param nframes   The amount of frames to be mixed.
 * \param play      The Playdata containing the playback state -- must not be
 *                  \c NULL.
 *
 * \return   The amount of frames actually mixed. This is always
 *           <= \a nframes.
 */
uint32_t Song_mix(Song* song, uint32_t nframes, Playdata* play);


/**
 * Skips part of the Song.
 *
 * \param song     The Song -- must not be \c NULL.
 * \param play     The Playdata -- must not be \c NULL.
 * \param amount   The amount of frames to be skipped.
 *
 * \return   The amount of frames actually skipped. This is <= \a amount.
 */
uint64_t Song_skip(Song* song, Playdata* play, uint64_t amount);


/**
 * Sets the mixing volume of the Song.
 *
 * \param song      The Song -- must not be \c NULL.
 * \param mix_vol   The volume -- must be finite or -INFINITY.
 */
void Song_set_mix_vol(Song* song, double mix_vol);


/**
 * Gets the mixing volume of the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The mixing volume.
 */
double Song_get_mix_vol(Song* song);


/**
 * Sets the initial subsong of the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 * \param num    The subsong number -- must be < \c KQT_SUBSONGS_MAX.
 */
void Song_set_subsong(Song* song, uint16_t num);


/**
 * Gets the initial subsong of the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The initial subsong number.
 */
uint16_t Song_get_subsong(Song* song);


/**
 * Sets the number of buffers in the Song.
 *
 * \param song    The Song -- must not be \c NULL.
 * \param count   The number of buffers -- must be > \c 0 and
 *                <= \c KQT_BUFFERS_MAX.
 *
 * \return   \c true if successful, or \c false if memory allocation failed.
 */
bool Song_set_buf_count(Song* song, int count);


/**
 * Gets the number of buffers in the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The number of buffers.
 */
int Song_get_buf_count(Song* song);


/**
 * Sets the size of buffers in the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 * \param size   The new size for a single buffer -- must be > \c 0.
 *
 * \return   \c true if successful, or \c false if memory allocation failed.
 */
bool Song_set_buf_size(Song* song, uint32_t size);


/**
 * Gets the size of a single buffer in the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The buffer size.
 */
uint32_t Song_get_buf_size(Song* song);


/**
 * Gets the buffers from the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The buffers.
 */
kqt_frame** Song_get_bufs(Song* song);


/**
 * Gets the Voice buffers from the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The Voice buffers.
 */
kqt_frame** Song_get_voice_bufs(Song* song);


/**
 * Gets the Subsong table from the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The Subsong table.
 */
Subsong_table* Song_get_subsongs(Song* song);


/**
 * Gets the Patterns of the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The Pattern table.
 */
Pat_table* Song_get_pats(Song* song);


/**
 * Gets the Instruments of the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The Instrument table.
 */
Ins_table* Song_get_insts(Song* song);


/**
 * Gets the array of Scales of the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The Scales.
 */
Scale** Song_get_scales(Song* song);


/**
 * Gets a Scale of the Song.
 *
 * \param song    The Song -- must not be \c NULL.
 * \param index   The Scale index -- must be >= 0 and < KQT_SCALES_MAX.
 *
 * \return   The Scale.
 */
Scale* Song_get_scale(Song* song, int index);


/**
 * Gets an indirect reference to the active Scale of the Song.
 *
 * \param song    The Song -- must not be \c NULL.
 *
 * \return   The reference.
 */
Scale** Song_get_active_scale(Song* song);


/**
 * Creates a new Scale for the Song.
 *
 * \param song    The Song -- must not be \c NULL.
 * \param index   The Scale index -- must be >= 0 and < KQT_SCALES_MAX.
 *
 * \return   \c true if successful, or \c false if memory allocation failed.
 */
bool Song_create_scale(Song* song, int index);


/**
 * Removes a Scale from the Song.
 *
 * \param song    The Song -- must not be \c NULL.
 * \param index   The Scale index -- must be >= 0 and < KQT_SCALES_MAX.
 *                If the Scale doesn't exist, nothing will be done.
 */
void Song_remove_scale(Song* song, int index);


/**
 * Gets the global Event queue of the Song.
 *
 * \param song   The Song -- must not be \c NULL.
 *
 * \return   The Event queue.
 */
Event_queue* Song_get_events(Song* song);


/**
 * Destroys an existing Song.
 *
 * \param song   The Song -- must not be \c NULL.
 */
void del_Song(Song* song);


#endif // K_SONG_H


