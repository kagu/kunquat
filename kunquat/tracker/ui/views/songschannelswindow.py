# -*- coding: utf-8 -*-

#
# Authors: Toni Ruottu, Finland 2014
#          Tomi Jylhä-Ollila, Finland 2015-2018
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from kunquat.tracker.ui.qt import *

from .chdefaultseditor import ChDefaultsEditor
from .orderlisteditor import OrderlistEditor
from .saverwindow import SaverWindow
from .songeditor import SongEditor
from .updater import Updater
from .utils import get_abs_window_size


class SongsChannelsWindow(Updater, SaverWindow):

    def __init__(self):
        super().__init__()
        self._orderlist_editor = OrderlistEditor()
        self._song_editor = SongEditor()
        self._ch_defaults_editor = ChDefaultsEditor()

        self.add_to_updaters(
                self._orderlist_editor, self._song_editor, self._ch_defaults_editor)

        self.setWindowTitle('Songs & channels')

        h = QHBoxLayout()
        h.setContentsMargins(4, 4, 4, 4)
        h.setSpacing(4)
        h.addWidget(self._orderlist_editor)
        h.addWidget(self._song_editor)
        h.addWidget(self._ch_defaults_editor)
        self.setLayout(h)

    def closeEvent(self, event):
        event.ignore()
        visibility_mgr = self._ui_model.get_visibility_manager()
        visibility_mgr.hide_songs_channels()

    def sizeHint(self):
        return get_abs_window_size(0.5, 0.7)


