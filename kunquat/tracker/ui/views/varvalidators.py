# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2015-2017
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from kunquat.tracker.ui.qt import *


class IntValidator(QValidator):

    def __init__(self):
        super().__init__()

    def validate(self, contents, pos):
        in_str = str(contents)
        if not in_str:
            return (QValidator.Intermediate, contents, pos)

        stripped = in_str.strip()
        if stripped in ('+', '-'):
            return (QValidator.Intermediate, contents, pos)

        try:
            value = int(stripped)
        except ValueError:
            return (QValidator.Invalid, contents, pos)

        return (QValidator.Acceptable, contents, pos)


class FloatValidator(QValidator):

    def __init__(self):
        super().__init__()

    def validate(self, contents, pos):
        in_str = str(contents)
        if not in_str:
            return (QValidator.Intermediate, contents, pos)

        stripped = in_str.strip()
        if stripped in ('+', '-', '.', '+.', '-.'):
            return (QValidator.Intermediate, contents, pos)

        try:
            value = float(in_str)
        except ValueError:
            return (QValidator.Invalid, contents, pos)

        return (QValidator.Acceptable, contents, pos)


