# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2016
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

import heapq
import math
import time
from itertools import chain, islice

from PySide.QtCore import *
from PySide.QtGui import *


DEFAULT_CONFIG = {
    'bg_colour'                 : QColor(0, 0, 0),
    'center_line_colour'        : QColor(0x66, 0x66, 0x66),
    'sample_colour'             : QColor(0x44, 0xcc, 0xff),
    'interp_colour'             : QColor(0x22, 0x88, 0xaa),
    'max_node_size'             : 6,
    'loop_line_colour'          : QColor(0x77, 0x99, 0xbb),
    'focused_loop_line_colour'  : QColor(0xff, 0xaa, 0x55),
    'loop_line_dash'            : [4, 4],
    'loop_handle_colour'        : QColor(0x88, 0xbb, 0xee),
    'focused_loop_handle_colour': QColor(0xff, 0xaa, 0x55),
    'loop_handle_size'          : 12,
    'loop_handle_focus_dist_max': 14,
}


class SampleView(QWidget):

    loopStartChanged = Signal(int, name='loopStartChanged')
    loopStopChanged = Signal(int, name='loopStopChanged')

    def __init__(self):
        super().__init__()

        self._toolbar = SampleViewToolBar()
        self._area = SampleViewArea(
                self._on_loop_start_changed, self._on_loop_stop_changed)

        v = QVBoxLayout()
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(0)
        v.addWidget(self._toolbar)
        v.addWidget(self._area)
        self.setLayout(v)

        QObject.connect(self._toolbar, SIGNAL('zoomIn()'), self._area.zoom_in)
        QObject.connect(self._toolbar, SIGNAL('zoomOut()'), self._area.zoom_out)

    def set_icon_bank(self, icon_bank):
        self._toolbar.set_icon_bank(icon_bank)

    def set_sample(self, length, get_sample_data):
        self._area.set_sample(length, get_sample_data)

    def set_loop_range(self, loop_range):
        self._area.set_loop_range(loop_range)

    def _on_loop_start_changed(self, lstart):
        QObject.emit(self, SIGNAL('loopStartChanged(int)'), lstart)

    def _on_loop_stop_changed(self, lstop):
        QObject.emit(self, SIGNAL('loopStopChanged(int)'), lstop)


class SampleViewToolBar(QToolBar):

    zoomIn = Signal(name='zoomIn')
    zoomOut = Signal(name='zoomOut')

    def __init__(self):
        super().__init__()

        self._zoom_in = QToolButton()
        self._zoom_in.setText('Zoom In')
        self._zoom_in.setToolTip(self._zoom_in.text())

        self._zoom_out = QToolButton()
        self._zoom_out.setText('Zoom Out')
        self._zoom_out.setToolTip(self._zoom_out.text())

        self.addWidget(self._zoom_in)
        self.addWidget(self._zoom_out)

        QObject.connect(self._zoom_in, SIGNAL('clicked()'), self._signal_zoom_in)
        QObject.connect(self._zoom_out, SIGNAL('clicked()'), self._signal_zoom_out)

    def set_icon_bank(self, icon_bank):
        self._zoom_in.setIcon(QIcon(icon_bank.get_icon_path('zoom_in')))
        self._zoom_out.setIcon(QIcon(icon_bank.get_icon_path('zoom_out')))

    def _signal_zoom_in(self):
        QObject.emit(self, SIGNAL('zoomIn()'))

    def _signal_zoom_out(self):
        QObject.emit(self, SIGNAL('zoomOut()'))


class SampleViewArea(QAbstractScrollArea):

    def __init__(self, signal_loop_start_changed, signal_loop_stop_changed):
        super().__init__()

        self.setViewport(SampleViewCanvas(
            signal_loop_start_changed, signal_loop_stop_changed))
        self.viewport().setFocusProxy(None)

        self.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOn)
        self.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

    def set_sample(self, length, get_sample_data):
        self.viewport().set_sample(length, get_sample_data)
        self._update_scrollbars()

    def set_loop_range(self, loop_range):
        self.viewport().set_loop_range(loop_range)

    def zoom_in(self):
        start, stop = self.viewport().get_range()
        width = stop - start
        shrink = width // 4
        shrink_l, shrink_r = shrink, shrink

        # Make sure that at least 8 frames are visible
        max_shrink_total = width - 8
        if shrink * 2 > max_shrink_total:
            shrink_l = max_shrink_total // 2
            shrink_r = max_shrink_total - shrink_l

        self.set_range(start + shrink_l, stop - shrink_r)

    def zoom_out(self):
        start, stop = self.viewport().get_range()
        length = self.viewport().get_length()
        width = stop - start
        expand = width // 2
        start -= expand
        stop += expand

        # Shift the expanded range if we went out of sample boundaries
        if start < 0:
            shift = -start
            start += shift
            stop += shift
        elif stop > length:
            shift = stop - length
            start -= shift
            stop -= shift

        self.set_range(start, stop)

    def set_range(self, start, stop):
        assert start < stop
        start = max(0, start)
        stop = min(self.viewport().get_length(), stop)
        self.viewport().set_range(start, stop)
        self._update_scrollbars()

    def _update_scrollbars(self):
        scrollbar = self.horizontalScrollBar()

        view = self.viewport()
        length = view.get_length()

        if length == 0:
            scrollbar.setRange(0, 0)
            return

        start, stop = view.get_range()
        width = stop - start
        single_step_px = 40
        scrollbar.setSingleStep(max(1, int(single_step_px * width / view.width())))
        scrollbar.setPageStep(width)
        scrollbar.setRange(0, length - width)
        scrollbar.setValue(start)

    def paintEvent(self, event):
        self.viewport().paintEvent(event)

    def scrollContentsBy(self, dx, dy):
        value = self.horizontalScrollBar().value()

        view = self.viewport()
        view.set_position(value)

    def mouseMoveEvent(self, event):
        self.viewport().mouseMoveEvent(event)

    def mousePressEvent(self, event):
        self.viewport().mousePressEvent(event)

    def mouseReleaseEvent(self, event):
        self.viewport().mouseReleaseEvent(event)

    def resizeEvent(self, event):
        self.viewport().resizeEvent(event)


class SampleViewCanvas(QWidget):

    refresh = Signal(name='refresh')

    _REF_PIXMAP_WIDTH = 256

    _STATE_IDLE = 'idle'
    _STATE_WAITING = 'waiting'
    _STATE_MOVING_MARKER = 'moving_marker'

    def __init__(self, signal_loop_start_changed, signal_loop_stop_changed, config={}):
        super().__init__()

        self._signal_loop_start_changed = signal_loop_start_changed
        self._signal_loop_stop_changed = signal_loop_stop_changed

        self._config = None
        self._set_config(config)

        self._length = 0
        self._range = [0, 0]
        self._loop_range = None

        self._focused_loop_marker = None
        self._moving_pointer_offset_x = 0

        self._state = self._STATE_IDLE

        self._shape_worker = ShapeWorker()
        self._pixmap_caches = {}

        self.setAutoFillBackground(False)
        self.setAttribute(Qt.WA_OpaquePaintEvent)
        self.setAttribute(Qt.WA_NoSystemBackground)

        self.setMouseTracking(True)

        QObject.connect(self, SIGNAL('refresh()'), self._refresh)

    def _set_config(self, config):
        self._config = DEFAULT_CONFIG.copy()
        self._config.update(config)

    def set_sample(self, length, get_sample_data):
        self._length = length
        self.set_range(0, length)
        self._loop_range = None

        self._shape_worker.reset(self._length, get_sample_data)
        self._pixmap_caches = {}

        self.update()

    def set_loop_range(self, loop_range):
        if self._loop_range != loop_range:
            self._loop_range = loop_range
            self.update()

        if self._state == self._STATE_WAITING:
            self._state = self._STATE_MOVING_MARKER

    def get_length(self):
        return self._length

    def set_range(self, start, stop):
        self._range = [start, stop]
        self.update()

    def get_range(self):
        return self._range

    def set_position(self, pos):
        start, stop = self._range
        pos_delta = pos - start
        start += pos_delta
        stop += pos_delta
        assert start >= 0
        assert stop <= self._length
        self._range = [start, stop]
        self.update()

    def _get_frame_start_vis_x(self, frame):
        start, stop = self._range
        range_width = stop - start
        frames_per_px = range_width / self.width()
        return (frame - start) / frames_per_px

    def _find_focused_loop_handle(self, pointer_pos):
        if not self._loop_range:
            return None

        # Shift focus position so that it matches better what is seen
        pos_x, pos_y = (pointer_pos[0] - 1, pointer_pos[1] - 1)

        lstart, lstop = self._loop_range
        lstart_x = self._get_frame_start_vis_x(lstart)
        lstop_x = self._get_frame_start_vis_x(lstop)

        dist_max = self._config['loop_handle_focus_dist_max']
        dist_size_diff = dist_max - self._config['loop_handle_size']

        dist_to_start = abs(lstart_x - pos_x) + pos_y
        if dist_to_start <= dist_max:
            return lstart

        dist_to_stop = abs(lstop_x - pos_x) + abs(self.height() - pos_y)
        if dist_to_stop <= dist_max:
            return lstop

        return None

    def mouseMoveEvent(self, event):
        pointer_pos = event.x(), event.y()

        if self._state == self._STATE_MOVING_MARKER:
            assert self._focused_loop_marker in self._loop_range

            pointer_x, _ = pointer_pos
            target_x = pointer_x + self._moving_pointer_offset_x

            # Find the nearest frame
            start, stop = self._range
            range_width = stop - start
            frames_per_px = range_width / self.width()

            target_frame = int(round(start + frames_per_px * target_x))
            lstart, lstop = self._loop_range
            if self._focused_loop_marker == lstart:
                target_frame = min(max(0, target_frame), min(self._length, lstop) - 1)
                self._signal_loop_start_changed(target_frame)
            else:
                target_frame = min(max(max(0, lstart) + 1, target_frame), self._length)
                self._signal_loop_stop_changed(target_frame)

            self._focused_loop_marker = target_frame
            self._state = self._STATE_WAITING

        elif self._state == self._STATE_IDLE:
            self._focused_loop_marker = self._find_focused_loop_handle(pointer_pos)
            self.update()

    def mousePressEvent(self, event):
        if event.buttons() != Qt.LeftButton:
            return

        if self._state != self._STATE_IDLE:
            return

        pointer_pos = event.x(), event.y()
        self._focused_loop_marker = self._find_focused_loop_handle(pointer_pos)
        if self._focused_loop_marker != None:
            self._state = self._STATE_MOVING_MARKER
            marker_x = self._get_frame_start_vis_x(self._focused_loop_marker)
            self._moving_pointer_offset_x = marker_x - event.x()

    def mouseReleaseEvent(self, event):
        self._state = self._STATE_IDLE

        pointer_pos = event.x(), event.y()
        self._focused_loop_marker = self._find_focused_loop_handle(pointer_pos)
        self.update()

    def _get_ref_frames_per_px(self, frames_per_px):
        return 2**math.floor(math.log(frames_per_px, 2))

    def _get_visible_pixmaps(self):
        height = self.height()

        # Get frames per pixel
        start, stop = self._range
        range_width = stop - start
        frames_per_px = range_width / self.width()

        # Get range width covered by a single shape slice
        ref_fpp = self._get_ref_frames_per_px(frames_per_px)
        pixmap_range_width = int(ref_fpp * self._REF_PIXMAP_WIDTH)

        src_rect = QRect(0, 0, self._REF_PIXMAP_WIDTH, height)
        dest_rect_width = (ref_fpp / frames_per_px) * self._REF_PIXMAP_WIDTH

        # Get pixmap indices
        start_i = start // pixmap_range_width
        stop_i = 1 + stop // pixmap_range_width

        # Get our pixmap cache
        pc = self._pixmap_caches.get(ref_fpp, {})
        self._pixmap_caches[ref_fpp] = pc

        view_start = start / frames_per_px

        # Update worker
        self._shape_worker.update()

        refresh_required = False

        for i in range(start_i, stop_i):
            start_x = int(i * dest_rect_width - view_start)
            stop_x = int((i + 1) * dest_rect_width - view_start)

            dest_rect = QRect(start_x, 0, stop_x - start_x, height)
            if i not in pc:
                slice_start = i * pixmap_range_width
                slice_stop = (i + 1) * pixmap_range_width
                if slice_start >= stop:
                    break

                shape = self._shape_worker.request_shape(
                        ref_fpp, (slice_start, slice_stop))

                if shape:
                    pixmap = QPixmap(self._REF_PIXMAP_WIDTH, height)
                    pixmap.fill(QColor(0, 0, 0))

                    painter = QPainter(pixmap)
                    painter.setRenderHint(QPainter.Antialiasing)
                    painter.translate(0.5, 0.5)
                    painter.scale(1, height / 2)
                    painter.translate(0, 1)
                    shape.draw_shape(painter, self._config)

                    '''
                    painter = QPainter(pixmap)
                    painter.setPen(QColor(0xff, 0xff, 0xff))
                    test_rect = src_rect.adjusted(0, 0, -1, -1)
                    painter.drawRect(test_rect)

                    painter.drawText(2, 14, '[{}, {})'.format(slice_start, slice_stop))
                    '''

                    pc[i] = pixmap
                else:
                    refresh_required = True

            if i in pc:
                yield dest_rect, src_rect, pc[i]

        # Schedule another update if we didn't get all the required image data yet
        if refresh_required:
            QObject.emit(self, SIGNAL('refresh()'))

    def _has_valid_loop_range(self):
        if not self._loop_range:
            return False

        lstart, lstop = self._loop_range
        return 0 <= lstart < lstop <= self._length

    def paintEvent(self, event):
        start = time.time()

        painter = QPainter(self)
        painter.setBackground(QColor(0, 0, 0))
        painter.eraseRect(0, 0, self.width(), self.height())
        painter.setRenderHint(QPainter.SmoothPixmapTransform)

        if self._length == 0:
            return

        # Draw waveform
        for dest_rect, src_rect, pixmap in self._get_visible_pixmaps():
            painter.drawPixmap(dest_rect, pixmap, src_rect)

        # Draw loop markers
        if self._has_valid_loop_range():
            lstart, lstop = self._loop_range

            width = self.width()
            height = self.height()

            # Draw lines
            def get_line_colour(pos):
                normal_colour = self._config['loop_line_colour']
                focused_colour = self._config['focused_loop_line_colour']
                return (focused_colour if self._focused_loop_marker == pos
                        else normal_colour)

            pen = QPen()
            pen.setDashPattern(self._config['loop_line_dash'])

            # Make sure we draw the focused line on top
            first_pos, second_pos = lstart, lstop
            if self._focused_loop_marker == lstart:
                first_pos, second_pos = second_pos, first_pos

            for pos in (first_pos, second_pos):
                vis_x = self._get_frame_start_vis_x(pos)
                if 0 <= vis_x < width:
                    pen.setColor(get_line_colour(pos))
                    painter.setPen(pen)
                    painter.drawLine(vis_x, 0, vis_x, height)

            # Draw handles
            def get_handle_colour(pos):
                normal_colour = self._config['loop_handle_colour']
                focused_colour = self._config['focused_loop_handle_colour']
                return (focused_colour if self._focused_loop_marker == pos
                        else normal_colour)

            painter.setPen(Qt.NoPen)
            handle_size = self._config['loop_handle_size']

            lstart_x = self._get_frame_start_vis_x(lstart)
            if -handle_size < lstart_x < width + handle_size:
                painter.setBrush(get_handle_colour(lstart))
                painter.drawConvexPolygon(QPolygon([
                    QPoint(lstart_x - handle_size + 1, 0),
                    QPoint(lstart_x + handle_size, 0),
                    QPoint(lstart_x, handle_size)]))

            lstop_x = self._get_frame_start_vis_x(lstop)
            if -handle_size < lstop_x < width + handle_size:
                painter.setBrush(get_handle_colour(lstop))
                painter.drawConvexPolygon(QPolygon([
                    QPoint(lstop_x - handle_size, height),
                    QPoint(lstop_x + handle_size + 1, height),
                    QPoint(lstop_x, height - handle_size)]))

        end = time.time()
        elapsed = end - start
        #print('Sample view updated in {:.2f} ms'.format(elapsed * 1000))

    def resizeEvent(self, event):
        self.update()

    def _refresh(self):
        self.update()


class DummyShape():
    '''Temporary type for testing purposes'''


class Shape():

    def __init__(self):
        self._shapes = []
        self._center_line_length = 0

        # Additional information required when displaying individual items
        self._items = []
        self._frame_step = 0

    def draw_shape(self, painter, config):
        if not self._shapes:
            return

        sample_colour = config['sample_colour']

        ch_count = len(self._shapes)

        painter.save()
        painter.scale(1, 1 / ch_count)
        painter.translate(0, -ch_count + 1)

        for i, shape in enumerate(self._shapes):
            # Center line
            painter.setPen(config['center_line_colour'])
            painter.drawLine(QPointF(0, 0), QPointF(self._center_line_length, 0))

            if isinstance(shape, QPolygonF):
                # Filled blob
                painter.setPen(sample_colour)
                painter.setBrush(sample_colour)
                painter.drawPolygon(shape)

            elif isinstance(shape, QPainterPath):
                # Line that connects the samples
                painter.setPen(config['interp_colour'])
                painter.setBrush(Qt.NoBrush)
                painter.drawPath(shape)

                # Get transformed positions of individual items
                tfm = painter.transform()
                item_points = []
                for item_i, val in enumerate(self._items[i]):
                    item_points.append(tfm.map(QPointF(item_i, -val)))

                # Draw items
                node_width = min(max(2.0, self._frame_step / 2), config['max_node_size'])
                node_height = 0.9 * node_width
                painter.save()
                painter.setTransform(QTransform())
                for point in item_points:
                    x, y = point.x() * self._frame_step, point.y()
                    painter.fillRect(
                            QRectF(x - node_width / 2, y - node_height / 2,
                                node_width, node_height),
                            sample_colour)
                painter.restore()

            painter.translate(0, 2)

        painter.restore()

    def make_shape(self, sample_data, ref_fpp, slice_range):
        if not sample_data:
            return

        start, stop = slice_range
        slice_range_width = stop - start
        assert slice_range_width > 0
        actual_stop = min(stop, len(sample_data[0]))

        self._center_line_length = slice_range_width / ref_fpp

        if ref_fpp > 2:
            # Filled blob
            ref_fpp_i = int(ref_fpp)
            for ch_data in sample_data:
                min_vals = []
                max_vals = []

                subslice_start = start
                while subslice_start < actual_stop:
                    subslice_stop = min(subslice_start + ref_fpp_i, actual_stop)
                    sl = ch_data[subslice_start:subslice_stop]
                    min_val = max(-1.0, min(sl))
                    max_val = min(1.0, max(sl))
                    min_vals.append(-min_val)
                    max_vals.append(-max_val)
                    subslice_start = subslice_stop

                min_vals = list(reversed(min_vals))
                sample_count = len(min_vals)

                points = [QPointF(i, val) for i, val in enumerate(max_vals)]
                points.extend([QPointF(sample_count - i - 1, val)
                    for i, val in enumerate(min_vals)])

                shape = QPolygonF(points)
                self._shapes.append(shape)

        else:
            # Line
            self._frame_step = 1 / ref_fpp
            margin = 2

            for ch_data in sample_data:
                shape = QPainterPath()

                prev_vals = list(ch_data[max(0, start - margin):start])
                if len(prev_vals) < margin:
                    prev_vals = ([0.0] * (margin - len(prev_vals))) + prev_vals

                tail_vals = list(ch_data[stop:stop + margin])
                if len(tail_vals) < margin:
                    tail_vals.extend([0.0] * (margin - len(tail_vals)))

                prev_point = QPointF((-margin + 0.5) * self._frame_step, -prev_vals[0])
                shape.moveTo(prev_point)

                items = ch_data[start:stop]
                self._items.append(items)

                vals = chain(prev_vals, ch_data[start:stop], tail_vals)
                for i, val in islice(enumerate(vals), 1, None):
                    val = min(max(-1, -val), 1)
                    point = QPointF((i - margin + 0.5) * self._frame_step, val)
                    shape.lineTo(point)

                self._shapes.append(shape)


class ShapeWorker():

    def __init__(self):
        self._shapes = {}
        self._requests = []
        self._cur_shape_prio = 0

        self._sample_length = 0
        self._get_sample_data = None
        self._sample_data = []

    def reset(self, length, get_sample_data):
        self._sample_length = length
        self._get_sample_data = get_sample_data
        self._sample_data = []

        self._shapes = {}
        self._requests = []
        self._cur_shape_prio = 0

    def _try_make_shape(self, ref_fpp, slice_range):
        start, stop = slice_range
        assert start < self._sample_length

        # Get new sample data if needed
        margin = 2
        if not self._sample_data or (len(self._sample_data[0]) < stop + margin):
            new_data = self._get_sample_data()
            if not self._sample_data:
                self._sample_data = list(new_data)
            else:
                assert len(self._sample_data) == len(new_data)
                for i, new_ch_data in enumerate(new_data):
                    self._sample_data[i].extend(new_ch_data)

        read_count = len(self._sample_data[0]) if self._sample_data else 0
        if read_count < stop + margin and read_count < self._sample_length:
            return None

        shape = Shape()
        shape.make_shape(self._sample_data, ref_fpp, slice_range)
        return shape

    def update(self):
        if not self._requests:
            self._cur_shape_prio = 0
            return

        # Give priority to future requests as they are
        # probably more relevant to current view
        self._cur_shape_prio -= 1

        time_limit = 0.01
        start_time = None

        while (start_time == None or
                time.time() - start_time < time_limit) and self._requests:
            if start_time == None:
                start_time = time.time()

            prio, request = heapq.heappop(self._requests)

            ref_fpp, slice_range = request
            shape = self._try_make_shape(ref_fpp, slice_range)
            if shape:
                key = (ref_fpp, slice_range)
                assert key not in self._shapes
                self._shapes[key] = shape
            else:
                # Shape is not finished yet, so push the request back
                heapq.heappush(self._requests, (prio + 1, request))

    def request_shape(self, ref_fpp, slice_range):
        # Return a completed shape
        key = (ref_fpp, slice_range)
        if key in self._shapes:
            return self._shapes[key]

        # Check if we are working on the requested shape
        if any(1 for p, k in self._requests if k == key):
            return None

        # Add request for a new shape
        heapq.heappush(self._requests, (self._cur_shape_prio, key))
        return None


