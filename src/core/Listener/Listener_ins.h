

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


#ifndef K_LISTENER_INS_H
#define K_LISTENER_INS_H


#include "Listener.h"


/**
 * The Instrument part of the Listener will respond to most methods with the
 * response method <host_path>/ins_info which contains the following arguments
 * if the host call has succeeded:
 *
 * \li \c i   The Song ID.
 * \li \c i   The Instrument number (1..255).
 * \li \c i   The type of the Instrument.
 * \li \c s   The name of the Instrument.
 * \li        Zero or more additional arguments dependent on the Instrument type.
 *
 * The None, Debug and Sine types have no additional arguments.
 *
 * The PCM type adds the following arguments for each existing Sample:
 *
 * \li \c i   The Sample number.
 * \li \c s   The path of the Sample.
 * \li \c d   The middle frequency of the Sample.
 */


/**
 * Finds the Instrument based on given Song ID and Instrument number.
 *
 * If the requested Song exists, it will be made current Song.
 *
 * \param l         The Listener -- must not be \c NULL.
 * \param song_id   The Song ID.
 * \param ins_num   The Instrument number.
 * \param ins       The location to which the Instrument will be stored
 *                  -- must not be \c NULL.
 *
 * \return   \c true if the Song exists and Instrument number is valid,
 *           otherwise \c false. Note that \c true will be returned if the
 *           search parameters are valid but no Instrument is found.
 */
bool ins_get(Listener* lr,
		int32_t song_id,
		int32_t ins_num,
		Instrument** ins);


/**
 * Sends the ins_info message.
 *
 * \param l         The Listener -- must not be \c NULL.
 * \param song_id   The Song ID.
 * \param ins_num   The Instrument number.
 * \param ins       The Instrument located in \a song_id, \a ins_num.
 *
 * \return   \c true if the message was sent successfully, otherwise \c false.
 */
bool ins_info(Listener* lr,
		int32_t song_id,
		int32_t ins_num,
		Instrument* ins);


/**
 * Gets information on all the Instruments of the given Song.
 *
 * The following OSC arguments are expected:
 *
 * \li \c i   The Song ID.
 *
 * The response consists of one ins_info method call for each existing
 * Instrument.
 */
Listener_callback Listener_get_insts;


/**
 * Creates a new Instrument.
 *
 * The following OSC arguments are expected:
 *
 * \li \c i   The Song ID.
 * \li \c i   The Instrument number (1..255). An Instrument with the same
 *            number will be removed if one exists.
 * \li \c i   The type of the new Instrument.
 */
Listener_callback Listener_new_ins;


/**
 * Sets the name of the Instrument.
 *
 * The following OSC arguments are expected:
 *
 * \li \c i   The Song ID.
 * \li \c i   The Instrument number (1..255).
 * \li \c s   The name.
 */
Listener_callback Listener_ins_set_name;


/**
 * Removes an Instrument.
 *
 * The following OSC arguments are expected:
 *
 * \li \c i   The Song ID.
 * \li \c i   The Instrument number (1..255).
 */
Listener_callback Listener_del_ins;


#endif // K_LISTENER_INS_H


