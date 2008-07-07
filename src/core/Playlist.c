

/*
 * Copyright 2008 Tomi Jylhä-Ollila
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
#include <assert.h>
#include <math.h>

#include "Playlist.h"

#include <xmemory.h>


Playlist* new_Playlist(void)
{
	Playlist* playlist = xalloc(Playlist);
	if (playlist == NULL)
	{
		return NULL;
	}
	playlist->buf_count = 2;
	playlist->buf_size = 128;
	playlist->first = NULL;
	Playlist_reset_stats(playlist);
	return playlist;
}


void Playlist_ins(Playlist* playlist, Player* player)
{
	assert(playlist != NULL);
	assert(player != NULL);
	assert(player->next == NULL);
	assert(player->prev == NULL);
	player->next = playlist->first;
	if (playlist->first != NULL)
	{
		playlist->first->prev = player;
	}
	player->prev = NULL;
	playlist->first = player;
	return;
}


Player* Playlist_get(Playlist* playlist, int32_t id)
{
	assert(playlist != NULL);
	Player* cur = playlist->first;
	while (cur != NULL && cur->id != id)
	{
		cur = cur->next;
	}
	return cur;
}


void Playlist_remove(Playlist* playlist, Player* player)
{
	assert(playlist != NULL);
	assert(player != NULL);
	Player* next = player->next;
	Player* prev = player->prev;
	if (next != NULL)
	{
		next->prev = prev;
	}
	if (prev != NULL)
	{
		prev->next = next;
	}
	if (player == playlist->first)
	{
		assert(prev == NULL);
		playlist->first = next;
	}
	del_Player(player);
	return;
}


bool Playlist_set_buf_count(Playlist* playlist, int count)
{
	assert(playlist != NULL);
	assert(count > 0);
	assert(count <= BUF_COUNT_MAX);
	Player* player = playlist->first;
	while (player != NULL)
	{
		if (!Song_set_buf_count(Player_get_song(player), count))
		{
			return false;
		}
		player = player->next;
	}
	playlist->buf_count = count;
	return true;
}


int Playlist_get_buf_count(Playlist* playlist)
{
	assert(playlist != NULL);
	return playlist->buf_count;
}


bool Playlist_set_buf_size(Playlist* playlist, uint32_t size)
{
	assert(playlist != NULL);
	assert(size > 0);
	Player* player = playlist->first;
	while (player != NULL)
	{
		if (!Song_set_buf_size(Player_get_song(player), size))
		{
			return false;
		}
		player = player->next;
	}
	playlist->buf_size = size;
	return true;
}


uint32_t Playlist_get_buf_size(Playlist* playlist)
{
	assert(playlist != NULL);
	return playlist->buf_size;
}


void Playlist_set_mix_freq(Playlist* playlist, uint32_t freq)
{
	assert(playlist != NULL);
	assert(freq > 0);
	Player* player = playlist->first;
	while (player != NULL)
	{
		Player_set_mix_freq(player, freq);
		player = player->next;
	}
	return;
}


void Playlist_reset_stats(Playlist* playlist)
{
	assert(playlist != NULL);
	for (int i = 0; i < BUF_COUNT_MAX; ++i)
	{
		playlist->max_values[i] = -INFINITY;
		playlist->min_values[i] = INFINITY;
	}
	Player* player = playlist->first;
	while (player != NULL)
	{
		Playdata* play = Player_get_playdata(player);
		Playdata_reset_stats(play);
		player = player->next;
	}
	return;
}


void del_Playlist(Playlist* playlist)
{
	assert(playlist != NULL);
	Player* target = playlist->first;
	playlist->first = NULL;
	while (target != NULL)
	{
		Player* next = target->next;
		del_Player(target);
		target = next;
	}
	xfree(playlist);
	return;
}


