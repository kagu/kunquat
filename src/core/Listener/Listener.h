

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


#ifndef K_LISTENER_H
#define K_LISTENER_H


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <Voice_pool.h>
#include <Playlist.h>

#include "lo/lo.h"

#include <xmemory.h>


#define METHOD_NAME_MAX (32)

#define MAX_VOICES (1024)


typedef enum
{
	LISTENER_ERROR_NONE = 0,
	LISTENER_ERROR_CREATE,
	LISTENER_ERROR_SELECT
} Listener_err;


typedef struct Listener
{
	/// Used to indicate exit.
	bool done;
	/// The OSC server.
	lo_server s;
	/// The file descriptor of the OSC server socket.
	int lo_fd;
	/// The OSC client address of the host application.
	lo_address host;
	/// The hostname (location) of the host application.
	char* host_hostname;
	/// The port of the host application.
	char* host_port;
	/// The path of the host application.
	char* host_path;
	/// The length of the host path.
	int host_path_len;
	/// The path of the method used.
	char* method_path;

	/// Current sound driver ID. Negative value implies none.
	int driver_id;

	/// The Voice pool used for mixing.
	Voice_pool* voices;
	/// Playback state information.
	Playlist* playlist;
	/// Player currently in use.
	Player* player_cur;
	/// Mixing frequency.
	uint32_t freq;
} Listener;


#define METHOD_PATH_ALLOC(full, path, method) do\
	{\
		(full) = xnalloc(char, strlen(path) + strlen(method) + 1);\
		if ((full) == NULL)\
		{\
			fprintf(stderr, "Out of memory at %s:%d\n", __FILE__, __LINE__);\
			return 0;\
		}\
		strcpy((full), (path));\
		strcat((full), (method));\
	} while (0)


/**
 * Registers a host application that uses Kunquat.
 *
 * The following OSC parameters are expected:
 * 
 * \li \c s   The OSC URL of the host application with base path.
 *
 * The Listener sends a confirmation message to the host on success.
 */
int Listener_register_host(const char* path,
		const char* types,
		lo_arg** argv,
		int argc,
		lo_message msg,
		void* user_data);


/**
 * Gets the Kunquat version.
 */
int Listener_version(const char* path,
		const char* types,
		lo_arg** argv,
		int argc,
		lo_message msg,
		void* user_data);


/**
 * Quits Kunquat.
 */
int Listener_quit(const char* path,
		const char* types,
		lo_arg** argv,
		int argc,
		lo_message msg,
		void* user_data);


/**
 * Shows all the OSC methods of Kunquat.
 */
int Listener_help(const char* path,
		const char* types,
		lo_arg** argv,
		int argc,
		lo_message msg,
		void* user_data);


/**
 * A fallback method. A host, if registered, will be sent a notification.
 */
int Listener_fallback(const char* path,
		const char* types,
		lo_arg** argv,
		int argc,
		lo_message msg,
		void* user_data);


/**
 * Set the number of Voices.
 *
 * The following OSC parameters are expected:
 *
 * \li \c i   The number of Voices. This should be > \c 0 and
 *            <= \c MAX_VOICES.
 */
int Listener_set_voices(const char* path,
		const char* types,
		lo_arg** argv,
		int argc,
		lo_message msg,
		void* user_data);


#endif // K_LISTENER_H

