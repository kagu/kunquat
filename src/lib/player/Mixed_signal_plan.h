

/*
 * Author: Tomi Jylhä-Ollila, Finland 2017-2019
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#ifndef KQT_MIXED_SIGNAL_PLAN_H
#define KQT_MIXED_SIGNAL_PLAN_H


#include <decl.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


/**
 * Create a new Mixed signal plan.
 *
 * \param dstates   The Device states -- must not be \c NULL.
 * \param conns     The Connections -- must not be \c NULL.
 *
 * \return   The new Mixed signal plan if successful, or \c NULL if memory
 *           allocation failed.
 */
Mixed_signal_plan* new_Mixed_signal_plan(
        Device_states* dstates, const Connections* conns);


/**
 * Execute all tasks in the Mixed signal plan.
 *
 * \param plan          The Mixed signal plan -- must not be \c NULL.
 * \param wbs           The Work buffers -- must not be \c NULL.
 * \param frame_count   Number of frames to be processed -- must not be greater than
 *                      the buffer size.
 * \param tempo         The current tempo -- must be > \c 0.
 */
void Mixed_signal_plan_execute_all_tasks(
        Mixed_signal_plan* plan, Work_buffers* wbs, int32_t frame_count, double tempo);


/**
 * Destroy an existing Mixed signal plan.
 *
 * \param plan   The Mixed signal plan, or \c NULL.
 */
void del_Mixed_signal_plan(Mixed_signal_plan* plan);


#endif // KQT_MIXED_SIGNAL_PLAN_H


