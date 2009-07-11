

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


#define _POSIX_SOURCE

#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <Handle_private.h>

#include <kunquat/limits.h>
#include <Song.h>
#include <Playdata.h>
#include <Voice_pool.h>

#include <xmemory.h>


// For errors without an associated Kunquat Handle.
static char null_error[KQT_CONTEXT_ERROR_LENGTH] = { '\0' };


kqt_Handle* kqt_new_Handle(long buffer_size)
{
    if (buffer_size <= 0)
    {
        kqt_Handle_set_error(NULL, "kqt_new_Handle: buf_size must be positive");
        return NULL;
    }
    kqt_Handle* handle = xalloc(kqt_Handle);
    if (handle == NULL)
    {
        kqt_Handle_set_error(NULL, "Couldn't allocate memory for a new Kunquat Handle");
        return NULL;
    }
    handle->song = NULL;
    handle->play = NULL;
    handle->play_silent = NULL;
    handle->voices = NULL;
    handle->error[0] = handle->error[KQT_CONTEXT_ERROR_LENGTH - 1] = '\0';
    handle->position[0] = handle->position[POSITION_LENGTH - 1] = '\0';

    int buffer_count = 2;
    int voice_count = 256;
    int event_queue_size = 32;

    handle->voices = new_Voice_pool(voice_count, event_queue_size);
    if (handle->voices == NULL)
    {
        kqt_del_Handle(handle);
        kqt_Handle_set_error(NULL, "Couldn't allocate memory for a new Kunquat Handle");
        return NULL;
    }

    handle->song = new_Song(buffer_count, buffer_size, event_queue_size);
    if (handle->song == NULL)
    {
        kqt_del_Handle(handle);
        kqt_Handle_set_error(NULL, "Couldn't allocate memory for a new Kunquat Handle");
        return NULL;
    }

    handle->play = new_Playdata(44100, handle->voices, Song_get_insts(handle->song));
    if (handle->play == NULL)
    {
        kqt_del_Handle(handle);
        kqt_Handle_set_error(NULL, "Couldn't allocate memory for a new Kunquat Handle");
        return NULL;
    }
    handle->play->order = Song_get_order(handle->song);
    handle->play->events = Song_get_events(handle->song);

    handle->play_silent = new_Playdata_silent(44100);
    if (handle->play_silent == NULL)
    {
        kqt_del_Handle(handle);
        kqt_Handle_set_error(NULL, "Couldn't allocate memory for a new Kunquat Handle");
        return NULL;
    }
    handle->play_silent->order = Song_get_order(handle->song);
    handle->play_silent->events = Song_get_events(handle->song);
    
    kqt_Handle_stop(handle);
    kqt_Handle_set_position(handle, NULL);
    return handle;
}


kqt_Handle* kqt_new_Handle_from_path(long buffer_size, char* path)
{
    if (buffer_size <= 0)
    {
        kqt_Handle_set_error(NULL, "kqt_new_Handle_from_path: buf_size must be positive");
        return NULL;
    }
    if (path == NULL)
    {
        kqt_Handle_set_error(NULL, "kqt_new_Handle_from_path: path must not be NULL");
        return NULL;
    }
    struct stat* info = &(struct stat){ .st_mode = 0 };
    errno = 0;
    if (stat(path, info) < 0)
    {
        kqt_Handle_set_error(NULL, "Couldn't access %s: %s", path, strerror(errno));
        return NULL;
    }
    kqt_Handle* handle = kqt_new_Handle(buffer_size);
    if (handle == NULL)
    {
        return NULL;
    }
    File_tree* tree = NULL;
    Read_state* state = READ_STATE_AUTO;
    if (S_ISDIR(info->st_mode))
    {
        tree = new_File_tree_from_fs(path, state);
        if (tree == NULL)
        {
            kqt_Handle_set_error(NULL, "%s:%d: %s",
                                  state->path, state->row, state->message);
            kqt_del_Handle(handle);
            return NULL;
        }
    }
    else
    {
        tree = new_File_tree_from_tar(path, state);
        if (tree == NULL)
        {
            kqt_Handle_set_error(NULL, "%s:%d: %s",
                                  state->path, state->row, state->message);
            kqt_del_Handle(handle);
            return NULL;
        }
    }
    assert(tree != NULL);
    if (!Song_read(handle->song, tree, state))
    {
        kqt_Handle_set_error(NULL, "%s:%d: %s",
                              state->path, state->row, state->message);
        del_File_tree(tree);
        kqt_del_Handle(handle);
        return NULL;
    }
    del_File_tree(tree);
    kqt_Handle_stop(handle);
    kqt_Handle_set_position(handle, NULL);
    return handle;
}


char* kqt_Handle_get_error(kqt_Handle* handle)
{
    if (handle == NULL)
    {
        return null_error;
    }
    return handle->error;
}


int kqt_Handle_get_subsong_length(kqt_Handle* handle, int subsong)
{
    if (handle == NULL)
    {
        kqt_Handle_set_error(NULL, "kqt_Handle_get_subsong_length: handle must not be NULL");
        return -1;
    }
    if (subsong < 0 || subsong >= KQT_SUBSONGS_MAX)
    {
        kqt_Handle_set_error(handle, "Invalid subsong number: %d", subsong);
        return -1;
    }
    assert(handle->song != NULL);
    Subsong* ss = Order_get_subsong(Song_get_order(handle->song), subsong);
    if (ss == NULL)
    {
        return 0;
    }
    return Subsong_get_length(ss);
}


void kqt_Handle_set_error(kqt_Handle* handle, char* message, ...)
{
    assert(message != NULL);
    char* error = null_error;
    if (handle != NULL)
    {
        error = handle->error;
    }
    va_list args;
    va_start(args, message);
    vsnprintf(error, KQT_CONTEXT_ERROR_LENGTH, message, args);
    va_end(args);
    error[KQT_CONTEXT_ERROR_LENGTH - 1] = '\0';
    return;
}


void kqt_del_Handle(kqt_Handle* handle)
{
    if (handle == NULL)
    {
        kqt_Handle_set_error(NULL, "kqt_del_Handle: handle must not be NULL");
        return;
    }
    if (handle->play_silent != NULL)
    {
        del_Playdata(handle->play_silent);
    }
    if (handle->play != NULL)
    {
        del_Playdata(handle->play);
    }
    if (handle->voices != NULL)
    {
        del_Voice_pool(handle->voices);
    }
    if (handle->song != NULL)
    {
        del_Song(handle->song);
    }
    xfree(handle);
    return;
}


