# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2010-2011
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

import re


FORMAT_VERSION = "00"

HANDLES_MAX = 256
KEY_LENGTH_MAX = 73
BUFFERS_MAX = 2
BUFFER_SIZE_MAX = 1048576
VOICES_MAX = 1024
SUBSONGS_MAX = 256
SECTIONS_MAX = 256
PATTERNS_MAX = 1024
COLUMNS_MAX = 64
TIMESTAMP_BEAT = 882161280
INSTRUMENTS_MAX = 256
GENERATORS_MAX = 8
EFFECTS_MAX = 256
INST_EFFECTS_MAX = 16
DSPS_MAX = 64
DEVICE_PORTS_MAX = GENERATORS_MAX
HITS_MAX = 128
SCALES_MAX = 16

SCALE_OCTAVES = 16
SCALE_MIDDLE_OCTAVE_UNBIASED = 8
SCALE_NOTE_MODS = 16
SCALE_NOTES = 128
SCALE_OCTAVE_BIAS = -3
SCALE_OCTAVE_FIRST = SCALE_OCTAVE_BIAS
SCALE_MIDDLE_OCTAVE = SCALE_MIDDLE_OCTAVE_UNBIASED + SCALE_OCTAVE_BIAS
SCALE_OCTAVE_LAST = SCALE_OCTAVES - 1 + SCALE_OCTAVE_BIAS

ARPEGGIO_NOTES_MAX = 64

key_format = re.compile('([0-9a-z_]+/)*[mp]_[0-9a-z_]+\\.[a-z]+')


def valid_key(s):
    return key_format.match(s)


