# -*- coding: utf-8 -*-

#
# Author: Toni Ruottu, Finland 2013
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

import unittest
import threading

from kunquat.tracker.audio.audio import Audio
from audiothread import AudioThread
from test_abstractthread import TestAbstractThread


class DummyFrontend(threading.Thread):
    pass


class DummyBackend(threading.Thread):
    pass


class TestAudiothread(TestAbstractThread, unittest.TestCase):

    def setUp(self):
        self._test_calls = [
          ('select_driver', ('coolsound',), {}),
          ('set_frontend', (DummyFrontend(),), {}),
          ('set_backend', (DummyBackend(),), {}),
          ('put_audio', (([0], [0]),), {}),
          ('request_update', (), {})
        ]
        self._InterfaceClass = Audio
        self._TestClass = AudioThread
        self._thread = self._TestClass()


if __name__ == '__main__':
    unittest.main()


