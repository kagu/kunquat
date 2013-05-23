# -*- coding: utf-8 -*-

#
# Authors: Tomi Jylhä-Ollila, Finland 2013
#          Toni Ruottu, Finland 2013
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

from PyQt4.QtCore import *
from PyQt4.QtGui import *

from driver_select import DriverSelect


class MainWindow(QWidget):

    def __init__(self):
        QWidget.__init__(self)
        self._ui_model = None

        self._driver_select = DriverSelect()

        self._progressBar = QProgressBar(self)
        self._progressBar.setGeometry(QRect(30, 70, 481, 23))
        self._progressBar.setObjectName("progressBar")
        self._progressBar.setValue(1)
        self._progressBar.setMaximum(1)
        self._progressBar.setMinimum(0)

        self._output_speed = QLabel(self)
        self._render_speed = QLabel(self)
        self._render_load = QLabel(self)

        v = QVBoxLayout()
        v.addWidget(self._driver_select)
        v.addWidget(self._progressBar)
        v.addWidget(self._output_speed)
        v.addWidget(self._render_speed)
        v.addWidget(self._render_load)
        self.setLayout(v)

    def set_ui_model(self, ui_model):
        self._ui_model = ui_model
        driver_manager = self._ui_model.get_driver_manager()
        self._driver_select.set_driver_manager(driver_manager)
        stats = self._ui_model.get_stat_manager()
        stats.register_updater(self.update_output_speed)
        stats.register_updater(self.update_render_speed)
        stats.register_updater(self.update_render_load)
        stats.register_updater(self.update_import_progress)

    def update_import_progress(self):
        stats = self._ui_model.get_stat_manager()
        position = stats.get_import_progress_position()
        steps = stats.get_import_progress_steps()
        self._progressBar.setMaximum(steps)
        self._progressBar.setValue(position)

    def update_output_speed(self):
        stats = self._ui_model.get_stat_manager()
        output_speed = stats.get_output_speed()
        text = 'output speed: {} fps'.format(int(output_speed))
        self._output_speed.setText(text)

    def update_render_speed(self):
        stats = self._ui_model.get_stat_manager()
        output_speed = stats.get_render_speed()
        text = 'render speed: {} fps'.format(int(output_speed))
        self._render_speed.setText(text)

    def update_render_load(self):
        stats = self._ui_model.get_stat_manager()
        render_load = stats.get_render_load()
        text = 'render load: {} %'.format(int(render_load * 100))
        self._render_load.setText(text)

    def run(self):
        self.update_import_progress()
        self.update_output_speed()
        self.update_render_speed()
        self.update_render_load()

    def __del__(self):
        pass


