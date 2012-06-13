

/*
 * Author: Tomi Jylhä-Ollila, Finland 2010
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <math.h>

#include <check.h>

#include <frame.h>
#include <Voice_state.h>
#include <Generator_debug.h>
#include <Instrument.h>
#include <Channel_state.h>


Suite* Instrument_suite(void);


START_TEST (new)
{
    kqt_frame buf_l[100] = { 0 };
    kqt_frame buf_r[100] = { 0 };
    kqt_frame* bufs[2] = { buf_l, buf_r };
    Scale* nts[KQT_SCALES_MAX] = { NULL };
    Scale** default_scale = &nts[0];
    Instrument* ins = new_Instrument(bufs, bufs, bufs, 2, 100, nts, &default_scale, 1);
    if (ins == NULL)
    {
        fprintf(stderr, "new_Instrument() returned NULL -- out of memory?\n");
        abort();
    }
    del_Instrument(ins);
}
END_TEST

#ifndef NDEBUG
START_TEST (new_break_bufs_null)
{
    Scale* nts[KQT_SCALES_MAX] = { NULL };
    Scale** default_scale = &nts[0];
    Instrument* ins = new_Instrument(NULL, NULL, NULL, 2, 1, nts, &default_scale, 1);
    del_Instrument(ins);
}
END_TEST

START_TEST (new_break_buf_len_inv)
{
    kqt_frame buf_l[1] = { 0 };
    kqt_frame buf_r[1] = { 0 };
    kqt_frame* bufs[2] = { buf_l, buf_r };
    Scale* nts[KQT_SCALES_MAX] = { NULL };
    Scale** default_scale = &nts[0];
    Instrument* ins = new_Instrument(bufs, bufs, bufs, 2, 0, nts, &default_scale, 1);
    del_Instrument(ins);
}
END_TEST

START_TEST (new_break_events_inv)
{
    kqt_frame buf_l[1] = { 0 };
    kqt_frame buf_r[1] = { 0 };
    kqt_frame* bufs[2] = { buf_l, buf_r };
    Scale* nts[KQT_SCALES_MAX] = { NULL };
    Scale** default_scale = &nts[0];
    Instrument* ins = new_Instrument(bufs, bufs, bufs, 2, 1, nts, &default_scale, 0);
    del_Instrument(ins);
}
END_TEST
#endif


START_TEST (mix)
{
    kqt_frame buf_l[128] = { 0 };
    kqt_frame buf_r[128] = { 0 };
    kqt_frame* bufs[2] = { buf_l, buf_r };
    kqt_frame* vbufs[2] = { buf_l, buf_r };
    Scale* nts[KQT_SCALES_MAX] = { NULL };
    Scale** default_scale = &nts[0];
    Instrument* ins = new_Instrument(bufs, vbufs, vbufs, 2, 128, nts, &default_scale, 16);
    if (ins == NULL)
    {
        fprintf(stderr, "new_Instrument() returned NULL -- out of memory?\n");
        abort();
    }
    Generator_debug* gen_debug = new_Generator_debug(Instrument_get_params(ins));
    if (gen_debug == NULL)
    {
        fprintf(stderr, "new_Generator_debug() returned NULL -- out of memory?\n");
        abort();
    }
    Instrument_set_gen(ins, 0, (Generator*)gen_debug);
    Voice_state state;
    bool mute = false;
    Channel_state ch_state;
    Channel_state_init(&ch_state, 0, &mute);
    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    state.pitch = 16;
    Instrument_mix(ins, &state, 128, 0, 64);
    fail_unless(!state.active,
            "Instrument didn't become inactive after finishing mixing.");
    for (int i = 0; i < 100; ++i)
    {
        if (i < 40)
        {
            if (i % 4 == 0)
            {
                fail_unless(bufs[0][i] > 0.99,
                        "Buffer contains %f at index %d (expected 1).", bufs[0][i], i);
            }
            else
            {
                fail_unless(bufs[0][i] > 0.49 && bufs[0][i] < 0.51,
                        "Buffer contains %f at index %d (expected 0.5).", bufs[0][i], i);
            }
        }
        else
        {
            fail_unless(fabs(bufs[0][i]) < 0.01,
                    "Buffer contains %f at index %d (expected 0).", bufs[0][i], i);
        }
    }
    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    for (int i = 0; i < 128; ++i)
    {
        buf_l[i] = 0;
        buf_r[i] = 0;
    }
    state.pitch = 16;
    for (int i = 0; i < 121; i += 7)
    {
        if (i < 40)
        {
            fail_unless(state.active,
                    "Instrument became inactive prematurely (after sample %d).", i);
        }
        else
        {
            fail_unless(!state.active,
                    "Instrument didn't become inactive after finishing mixing (at sample %d).", i);
            break;
        }
        Instrument_mix(ins, &state, i + 7, i, 64);
    }
    for (int i = 0; i < 128; ++i)
    {
        if (i < 40)
        {
            if (i % 4 == 0)
            {
                fail_unless(bufs[0][i] > 0.99,
                        "Buffer contains %f at index %d (expected 1).", bufs[0][i], i);
            }
            else
            {
                fail_unless(bufs[0][i] > 0.49 && bufs[0][i] < 0.51,
                        "Buffer contains %f at index %d (expected 0.5).", bufs[0][i], i);
            }
        }
        else
        {
            fail_unless(fabs(bufs[0][i]) < 0.01,
                    "Buffer contains %f at index %d (expected 0).", bufs[0][i], i);
        }
    }
    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    for (int i = 0; i < 128; ++i)
    {
        buf_l[i] = 0;
        buf_r[i] = 0;
    }
    state.pitch = 16;
    for (int i = 0; i < 127; ++i)
    {
        if (i < 40)
        {
            fail_unless(state.active,
                    "Instrument became inactive prematurely (after sample %d).", i);
        }
        else
        {
            fail_unless(!state.active,
                    "Instrument didn't become inactive after finishing mixing (at sample %d).", i);
            break;
        }
        Instrument_mix(ins, &state, i + 1, i, 64);
    }
    for (int i = 0; i < 128; ++i)
    {
        if (i < 40)
        {
            if (i % 4 == 0)
            {
                fail_unless(bufs[0][i] > 0.99,
                        "Buffer contains %f at index %d (expected 1).", bufs[0][i], i);
            }
            else
            {
                fail_unless(bufs[0][i] > 0.49 && bufs[0][i] < 0.51,
                        "Buffer contains %f at index %d (expected 0.5).", bufs[0][i], i);
            }
        }
        else
        {
            fail_unless(fabs(bufs[0][i]) < 0.01,
                    "Buffer contains %f at index %d (expected 0).", bufs[0][i], i);
        }
    }
    
    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    for (int i = 0; i < 128; ++i)
    {
        buf_l[i] = 0;
        buf_r[i] = 0;
    }
    state.pitch = 16;
    Instrument_mix(ins, &state, 20, 0, 64);
    state.note_on = false;
    Instrument_mix(ins, &state, 128, 20, 64);
    fail_unless(!state.active,
            "Instrument didn't become inactive after finishing mixing.");
    for (int i = 0; i < 20; ++i)
    {
        if (i % 4 == 0)
        {
            fail_unless(bufs[0][i] > 0.99,
                    "Buffer contains %f at index %d (expected 1).", bufs[0][i], i);
        }
        else
        {
            fail_unless(bufs[0][i] > 0.49 && bufs[0][i] < 0.51,
                    "Buffer contains %f at index %d (expected 0.5).", bufs[0][i], i);
        }
    }
    for (int i = 20; i < 28; ++i)
    {
        if (i % 4 == 0)
        {
            fail_unless(bufs[0][i] < -0.99,
                    "Buffer contains %f at index %d (expected -1).", bufs[0][i], i);
        }
        else
        {
            fail_unless(bufs[0][i] < -0.49 && bufs[0][i] > -0.51,
                    "Buffer contains %f at index %d (expected -0.5).", bufs[0][i], i);
        }
    }
    for (int i = 28; i < 128; ++i)
    {
        fail_unless(fabs(bufs[0][i]) < 0.01,
                "Buffer contains %f at index %d (expected 0).", bufs[0][i], i);
    }
    
    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    for (int i = 0; i < 128; ++i)
    {
        buf_l[i] = 0;
        buf_r[i] = 0;
    }
    state.pitch = 16;
    Instrument_mix(ins, &state, 36, 0, 64);
    state.note_on = false;
    Instrument_mix(ins, &state, 128, 36, 64);
    fail_unless(!state.active,
            "Instrument didn't become inactive after finishing mixing.");
    for (int i = 0; i < 36; ++i)
    {
        if (i % 4 == 0)
        {
            fail_unless(bufs[0][i] > 0.99,
                    "Buffer contains %f at index %d (expected 1).", bufs[0][i], i);
        }
        else
        {
            fail_unless(bufs[0][i] > 0.49 && bufs[0][i] < 0.51,
                    "Buffer contains %f at index %d (expected 0.5).", bufs[0][i], i);
        }
    }
    for (int i = 36; i < 40; ++i)
    {
        if (i % 4 == 0)
        {
            fail_unless(bufs[0][i] < -0.99,
                    "Buffer contains %f at index %d (expected -1).", bufs[0][i], i);
        }
        else
        {
            fail_unless(bufs[0][i] < -0.49 && bufs[0][i] > -0.51,
                    "Buffer contains %f at index %d (expected -0.5).", bufs[0][i], i);
        }
    }
    for (int i = 40; i < 128; ++i)
    {
        fail_unless(fabs(bufs[0][i]) < 0.01,
                "Buffer contains %f at index %d (expected 0).", bufs[0][i], i);
    }
    
    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    for (int i = 0; i < 128; ++i)
    {
        buf_l[i] = 1;
        buf_r[i] = 1;
    }
    state.pitch = 16;
    Instrument_mix(ins, &state, 36, 0, 64);
    state.note_on = false;
    Instrument_mix(ins, &state, 128, 36, 64);
    fail_unless(!state.active,
            "Instrument didn't become inactive after finishing mixing.");
    for (int i = 0; i < 36; ++i)
    {
        if (i % 4 == 0)
        {
            fail_unless(bufs[0][i] > 1.99,
                    "Buffer contains %f at index %d (expected 2).", bufs[0][i], i);
        }
        else
        {
            fail_unless(bufs[0][i] > 1.49 && bufs[0][i] < 1.51,
                    "Buffer contains %f at index %d (expected 1.5).", bufs[0][i], i);
        }
    }
    for (int i = 36; i < 40; ++i)
    {
        if (i % 4 == 0)
        {
            fail_unless(bufs[0][i] < 0.01,
                    "Buffer contains %f at index %d (expected 0).", bufs[0][i], i);
        }
        else
        {
            fail_unless(bufs[0][i] > 0.49 && bufs[0][i] < 0.51,
                    "Buffer contains %f at index %d (expected 0.5).", bufs[0][i], i);
        }
    }
    for (int i = 40; i < 128; ++i)
    {
        fail_unless(fabs(bufs[0][i]) < 1.01,
                "Buffer contains %f at index %d (expected 1).", bufs[0][i], i);
    }

    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    for (int i = 0; i < 128; ++i)
    {
        buf_l[i] = 0;
        buf_r[i] = 0;
    }
    state.pitch = 8;
    Instrument_mix(ins, &state, 128, 0, 64);
    fail_unless(!state.active,
            "Instrument didn't become inactive after finishing mixing.");
    for (int i = 0; i < 80; ++i)
    {
        if (i % 8 == 0)
        {
            fail_unless(bufs[0][i] > 0.99,
                    "Buffer contains %f at index %d (expected 1).", bufs[0][i], i);
        }
        else
        {
            fail_unless(bufs[0][i] > 0.49 && bufs[0][i] < 0.51,
                    "Buffer contains %f at index %d (expected 0.5).", bufs[0][i], i);
        }
    }
    for (int i = 80; i < 128; ++i)
    {
        fail_unless(fabs(bufs[0][i]) < 0.01,
                "Buffer contains %f at index %d (expected 0).", bufs[0][i], i);
    }

    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    for (int i = 0; i < 128; ++i)
    {
        buf_l[i] = 0;
        buf_r[i] = 0;
    }
    state.pitch = 8;
    Instrument_mix(ins, &state, 128, 0, 32);
    fail_unless(!state.active,
            "Instrument didn't become inactive after finishing mixing.");
    for (int i = 0; i < 40; ++i)
    {
        if (i % 4 == 0)
        {
            fail_unless(bufs[0][i] > 0.99,
                    "Buffer contains %f at index %d (expected 1).", bufs[0][i], i);
        }
        else
        {
            fail_unless(bufs[0][i] > 0.49 && bufs[0][i] < 0.51,
                    "Buffer contains %f at index %d (expected 0.5).", bufs[0][i], i);
        }
    }
    for (int i = 40; i < 128; ++i)
    {
        fail_unless(fabs(bufs[0][i]) < 0.01,
                "Buffer contains %f at index %d (expected 0).", bufs[0][i], i);
    }

    del_Instrument(ins);
}
END_TEST

#ifndef NDEBUG
START_TEST (mix_break_ins_null)
{
    Voice_state state;
    bool mute = false;
    Channel_state ch_state;
    Channel_state_init(&ch_state, 0, &mute);
    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    Instrument_mix(NULL, &state, 0, 0, 1);
}
END_TEST

START_TEST (mix_break_state_null)
{
    kqt_frame buf_l[1] = { 0 };
    kqt_frame buf_r[1] = { 0 };
    kqt_frame* bufs[2] = { buf_l, buf_r };
    kqt_frame* vbufs[2] = { buf_l, buf_r };
    Scale* nts[KQT_SCALES_MAX] = { NULL };
    Scale** default_scale = &nts[0];
    Instrument* ins = new_Instrument(bufs, vbufs, vbufs, 2, 1, nts, &default_scale, 1);
    if (ins == NULL)
    {
        fprintf(stderr, "new_Instrument() returned NULL -- out of memory?\n");
        return;
    }
    Generator_debug* gen_debug = new_Generator_debug(Instrument_get_params(ins));
    if (gen_debug == NULL)
    {
        fprintf(stderr, "new_Generator_debug() returned NULL -- out of memory?\n");
        abort();
    }
    Instrument_set_gen(ins, 0, (Generator*)gen_debug);
    Instrument_mix(ins, NULL, 0, 0, 1);
    del_Instrument(ins);
}
END_TEST

// XXX: enable again after implementing instrument buffers
#if 0
START_TEST (mix_break_nframes_inv)
{
    kqt_frame buf_l[1] = { 0 };
    kqt_frame buf_r[1] = { 0 };
    kqt_frame* bufs[2] = { buf_l, buf_r };
    kqt_frame* vbufs[2] = { buf_l, buf_r };
    Scale* nts[KQT_SCALES_MAX] = { NULL };
    Scale** default_scale = &nts[0];
    Instrument* ins = new_Instrument(bufs, vbufs, vbufs, 2, 1, nts, &default_scale, 1);
    if (ins == NULL)
    {
        fprintf(stderr, "new_Instrument() returned NULL -- out of memory?\n");
        return;
    }
    Generator_debug* gen_debug = new_Generator_debug(Instrument_get_params(ins));
    if (gen_debug == NULL)
    {
        fprintf(stderr, "new_Generator_debug() returned NULL -- out of memory?\n");
        abort();
    }
    Instrument_set_gen(ins, 0, (Generator*)gen_debug);
    Voice_state state;
    bool mute = false;
    Channel_state ch_state;
    Channel_state_init(&ch_state, 0, &mute);
    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    Instrument_mix(ins, &state, 2, 0, 1);
    del_Instrument(ins);
}
END_TEST
#endif

START_TEST (mix_break_freq_inv)
{
    kqt_frame buf_l[1] = { 0 };
    kqt_frame buf_r[1] = { 0 };
    kqt_frame* bufs[2] = { buf_l, buf_r };
    kqt_frame* vbufs[2] = { buf_l, buf_r };
    Scale* nts[KQT_SCALES_MAX] = { NULL };
    Scale** default_scale = &nts[0];
    Instrument* ins = new_Instrument(bufs, vbufs, vbufs, 2, 1, nts, &default_scale, 1);
    if (ins == NULL)
    {
        fprintf(stderr, "new_Instrument() returned NULL -- out of memory?\n");
        return;
    }
    Generator_debug* gen_debug = new_Generator_debug(Instrument_get_params(ins));
    if (gen_debug == NULL)
    {
        fprintf(stderr, "new_Generator_debug() returned NULL -- out of memory?\n");
        abort();
    }
    Instrument_set_gen(ins, 0, (Generator*)gen_debug);
    Voice_state state;
    bool mute = false;
    Channel_state ch_state;
    Channel_state_init(&ch_state, 0, &mute);
    Voice_state_init(&state, &ch_state, &ch_state, 64, 120);
    Instrument_mix(ins, &state, 1, 0, 0);
    del_Instrument(ins);
}
END_TEST
#endif


Suite* Instrument_suite(void)
{
    Suite* s = suite_create("Instrument");
    TCase* tc_new = tcase_create("new");
    TCase* tc_mix = tcase_create("mix");
    suite_add_tcase(s, tc_new);
    suite_add_tcase(s, tc_mix);

    int timeout = 10;
    tcase_set_timeout(tc_new, timeout);
    tcase_set_timeout(tc_mix, timeout);

    tcase_add_test(tc_new, new);
    tcase_add_test(tc_mix, mix);

#ifndef NDEBUG
    tcase_add_test_raise_signal(tc_new, new_break_bufs_null, SIGABRT);
    tcase_add_test_raise_signal(tc_new, new_break_buf_len_inv, SIGABRT);
    tcase_add_test_raise_signal(tc_new, new_break_events_inv, SIGABRT);

    tcase_add_test_raise_signal(tc_mix, mix_break_ins_null, SIGABRT);
    tcase_add_test_raise_signal(tc_mix, mix_break_state_null, SIGABRT);
//  tcase_add_test_raise_signal(tc_mix, mix_break_nframes_inv, SIGABRT);
    tcase_add_test_raise_signal(tc_mix, mix_break_freq_inv, SIGABRT);
#endif

    return s;
}


int main(void)
{
    int fail_count = 0;
    Suite* s = Instrument_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    fail_count = srunner_ntests_failed(sr);
    srunner_free(sr);
    if (fail_count > 0)
    {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

