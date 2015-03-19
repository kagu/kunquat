

/*
 * Author: Tomi Jylhä-Ollila, Finland 2013-2015
 *
 * This file is part of Kunquat.
 *
 * CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
 *
 * To the extent possible under law, Kunquat Affirmers have waived all
 * copyright and related or neighboring rights to Kunquat.
 */


#include <handle_utils.h>
#include <test_common.h>

#include <kunquat/Handle.h>
#include <kunquat/Player.h>


#define buf_len 128


START_TEST(Trivial_chorus_is_identity)
{
    set_audio_rate(220);
    set_mix_volume(0);
    pause();

    set_data("ins_03/prc_01/c/voice_00/p_f_delay.json", "0");
    set_data("ins_03/prc_01/c/voice_00/p_f_range.json", "0");
    set_data("ins_03/prc_01/c/voice_00/p_f_speed.json", "0");
    set_data("ins_03/prc_01/c/voice_00/p_f_volume.json", "0");
    set_data("ins_03/prc_01/p_prc_type.json", "\"chorus\"");
    set_data("ins_03/prc_01/in_00/p_manifest.json", "{}");
    set_data("ins_03/prc_01/out_00/p_manifest.json", "{}");
    set_data("ins_03/prc_01/p_manifest.json", "{}");

    set_data("ins_03/p_connections.json",
            "[ [\"in_00\", \"prc_01/C/in_00\"], "
            "  [\"prc_01/C/out_00\", \"out_00\"] ]");
    set_data("ins_03/in_00/p_manifest.json", "{}");
    set_data("ins_03/out_00/p_manifest.json", "{}");
    set_data("ins_03/p_manifest.json", "{}");

    set_data("ins_02/prc_00/p_prc_type.json", "\"debug\"");
    set_data("ins_02/prc_00/out_00/p_manifest.json", "{}");
    set_data("ins_02/prc_00/p_manifest.json", "{}");
    set_data("ins_02/p_manifest.json", "{}");
    set_data("ins_02/out_00/p_manifest.json", "{}");
    set_data("ins_02/p_connections.json",
            "[ [\"prc_00/C/out_00\", \"out_00\"] ]");

    set_data("out_00/p_manifest.json", "{}");
    set_data("p_connections.json",
            "[ [\"ins_02/out_00\", \"ins_03/in_00\"], "
            "  [\"ins_03/out_00\", \"out_00\"] ]");
    set_data("p_control_map.json", "[ [0, 2] ]");
    set_data("control_00/p_manifest.json", "{}");

    validate();

    float actual_buf[buf_len] = { 0.0f };
    kqt_Handle_fire_event(handle, 0, Note_On_55_Hz);
    check_unexpected_error();
    mix_and_fill(actual_buf, buf_len);

    float expected_buf[buf_len] = { 0.0f };
    float seq[] = { 1.0f, 0.5f, 0.5f, 0.5f };
    repeat_seq_local(expected_buf, 10, seq);

    check_buffers_equal(expected_buf, actual_buf, buf_len, 0.0f);
}
END_TEST


Suite* DSP_suite(void)
{
    Suite* s = suite_create("DSP");

    const int timeout = DEFAULT_TIMEOUT;

    TCase* tc_chorus = tcase_create("chorus");
    suite_add_tcase(s, tc_chorus);
    tcase_set_timeout(tc_chorus, timeout);
    tcase_add_checked_fixture(tc_chorus, setup_empty, handle_teardown);

    tcase_add_test(tc_chorus, Trivial_chorus_is_identity);

    return s;
}


int main(void)
{
    Suite* suite = DSP_suite();
    SRunner* sr = srunner_create(suite);
#ifdef K_MEM_DEBUG
    srunner_set_fork_status(sr, CK_NOFORK);
#endif
    srunner_run_all(sr, CK_NORMAL);
    int fail_count = srunner_ntests_failed(sr);
    srunner_free(sr);
    exit(fail_count > 0);
}



