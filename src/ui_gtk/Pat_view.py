# coding=utf-8


# Copyright 2008 Tomi Jylhä-Ollila
#
# This file is part of Kunquat.
#
# Kunquat is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Kunquat is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Kunquat.  If not, see <http://www.gnu.org/licenses/>.


import pygtk
pygtk.require('2.0')
import gtk, gobject, cairo, pango
from gtk import gdk
import math


RELTIME_FULL_PART = 882161280
COLUMNS = 64

class Ev:

	def __init__(self):
		i = 0
		events = (
			('NONE', 0),
			'GENERAL_COND',
			('GENERAL_LAST', 63),
			'GLOBAL_SET_VAR',
			'GLOBAL_TEMPO',
			'GLOBAL_VOLUME',
			('GLOBAL_LAST', 127),
			'NOTE_ON',
			'NOTE_OFF',
			'LAST')
		for x in events:
			if type(x) == type(()):
				setattr(self, *x)
				i = x[1]
			else:
				setattr(self, x, i)
			i += 1

	def is_general(self, t):
		return self.NONE < t < self.GENERAL_LAST

	def is_global(self, t):
		return self.GENERAL_LAST < t < self.GLOBAL_LAST

	def is_ins(self, t):
		return self.GLOBAL_LAST < t < self.LAST

	def is_valid(self, t):
		return self.is_general(t) or self.is_global(t) or self.is_ins(t)

evtype = Ev()


def time_normalise(t):
	if t[1] >= RELTIME_FULL_PART:
		t = (t[0] + 1, t[1] - RELTIME_FULL_PART)
	elif t[1] < 0:
		t = (t[0] - 1, t[1] + RELTIME_FULL_PART)
	return t

def time_add(t1, t2):
	res = (t1[0] + t2[0], t1[1] + t2[1])
	return time_normalise(res)

def time_sub(t1, t2):
	return time_add(t1, (-t2[0], -t2[1]))


class Pat_info:
	
	def __init__(self, num, len):
		self.num = num
		self.len = len
		self.cols = [None for _ in range(COLUMNS + 1)]


class Pat_view(gtk.Widget):

	def pat_info(self, path, args, types):
		self.pdata = Pat_info(args[0], (args[1], args[2]))
		if self.cursor[0][:2] > self.pdata.len:
			self.cursor = (self.pdata.len + (0,), self.cursor[1])

	def event_info(self, path, args, types):
		if self.pdata.cols[args[1]] == None:
			self.pdata.cols[args[1]] = {}
		self.pdata.cols[args[1]][(args[2], args[3], args[4])] = args[5:]

	def events_sent(self, path, args, types):
		self.do_redraw()

#	def handle_motion_notify(self, widget, event):
#		print('motion')
#		return True

	def handle_button_press(self, widget, event):
		print('button')
		self.grab_focus()
		return True

	def handle_focus_in(self, widget, event):
		print('focus in')
		return True

	def handle_focus_out(self, widget, event):
		print('focus out')
		return True

	def handle_key(self, widget, event):
		if not event.type == gdk.KEY_PRESS:
			return False
		key_name = gdk.keyval_name(event.keyval)
		if self.pdata == None:
			return True
		if key_name == 'Down':
			if event.state & gdk.CONTROL_MASK:
				self.zoom(1.0/1.5)
				self.queue_draw()
			elif self.tmove != 0 and 0 <= self.snap_state < self.snap_delay:
				self.snap_state = max(-1, self.snap_state - 1)
				if self.snap_state < 0:
					self.snap_delay = max(0, self.snap_delay - 1)
					self.snap_state = self.snap_delay
			else:
				init_move = False
				if self.tmove <= 0:
					init_move = True
					self.tmove = 0.8
				elif self.tmove < 16:
					self.tmove *= 1.3
				else:
					self.tmove = 16
				ctime = time_add(self.cursor[0][:2], self.px_time(self.tmove))
				if ctime == self.cursor[0][:2]:
					ctime = time_add(ctime, (0, 1))
				events = []
				if self.pdata.cols[self.cursor[1]]:
					events = [(k, v) for (k, v) in self.pdata.cols[self.cursor[1]].iteritems()
							if self.cursor[0][:2] < k[:2] <= ctime]
				if events:
					first = min(events)
					ctime = first[0][:2]
					if not init_move:
						self.snap_state = self.snap_delay - 1
				self.cursor = (ctime + (self.cursor[0][2],), self.cursor[1])
				self.queue_draw()
		elif key_name == 'Up':
			if event.state & gdk.CONTROL_MASK:
				self.zoom(1.5)
				self.queue_draw()
			elif self.tmove != 0 and 0 <= self.snap_state < self.snap_delay:
				self.snap_state = max(-1, self.snap_state - 1)
				if self.snap_state < 0:
					self.snap_delay = max(0, self.snap_delay - 1)
					self.snap_state = self.snap_delay
			else:
				init_move = False
				if self.tmove >= 0:
					init_move = True
					self.tmove = -0.8
				elif self.tmove > -16:
					self.tmove *= 1.3
				else:
					self.tmove = -16
				ctime = time_add(self.cursor[0][:2], self.px_time(self.tmove))
				if ctime == self.cursor[0][:2]:
					ctime = time_sub(ctime, (0, 1))
				events = []
				if self.pdata.cols[self.cursor[1]]:
					events = [(k, v) for (k, v) in self.pdata.cols[self.cursor[1]].iteritems()
							if ctime <= k < self.cursor[0][:2]]
				if events:
					last = max(events)
					ctime = last[0][:2]
					if not init_move:
						self.snap_state = self.snap_delay - 1
				self.cursor = (ctime + (self.cursor[0][2],), self.cursor[1])
				self.queue_draw()
		elif key_name == 'Left':
			if self.pdata.cols[self.cursor[1]]:
				if self.cursor[0] in self.pdata.cols[self.cursor[1]]:
					self.cursor = (self.cursor[0][:2] +
							(max(self.cursor[0][2] - 1, 0),), self.cursor[1])
				else:
					self.cursor = (self.cursor[0][:2] + (0,), self.cursor[1])
				self.queue_draw()
		elif key_name == 'Right':
			if self.pdata.cols[self.cursor[1]]:
				if self.cursor[0] in self.pdata.cols[self.cursor[1]]:
					self.cursor = (self.cursor[0][:2] +
							(self.cursor[0][2] + 1,), self.cursor[1])
				else:
					self.cursor = (self.cursor[0][:2] + (0,), self.cursor[1])
				self.queue_draw()
		elif key_name == 'Page_Down':
			self.cursor = ((self.cursor[0][0] + 4,) + self.cursor[0][1:], self.cursor[1])
			self.queue_draw()
		elif key_name == 'Page_Up':
			ctime = time_normalise((self.cursor[0][0] - 4, self.cursor[0][1]))
			self.cursor = (ctime + (self.cursor[0][2],), self.cursor[1])
			self.queue_draw()
		elif key_name == 'Tab':
			if self.cursor[1] < COLUMNS:
				self.cursor = (self.cursor[0], self.cursor[1] + 1)
				self.queue_draw()
		elif key_name == 'ISO_Left_Tab':
			if self.cursor[1] > 0:
				self.cursor = (self.cursor[0], self.cursor[1] - 1)
				self.queue_draw()
		else:
			print('press %s' % key_name)
		return True

	def handle_release(self, widget, event):
		key_name = gdk.keyval_name(event.keyval)
		if key_name == 'Down' or key_name == 'Up':
			self.tmove = 0
			self.snap_state = self.snap_delay = self.snap_init_delay
		return True

	def handle_scroll(self, widget, event):
		if event.direction == gdk.SCROLL_UP:
			self.zoom(1.5)
			self.queue_draw()
		elif event.direction == gdk.SCROLL_DOWN:
			self.zoom(1.0/1.5)
			self.queue_draw()
		return True

	def zoom(self, factor):
		new_ppb = self.pixels_per_beat * factor
		if new_ppb < 1:
			new_ppb = 1
		elif new_ppb > RELTIME_FULL_PART * 4:
			new_ppb = RELTIME_FULL_PART * 4
		cur_loc = self.time_px(time_sub(self.cursor[0][:2], self.view_corner[0]))
		self.pixels_per_beat = new_ppb
		new_loc = self.px_time(cur_loc)
		self.view_corner = (time_sub(self.cursor[0][:2], new_loc), self.view_corner[1])

	def do_realize(self):
		self.set_flags(self.flags()
				| gtk.REALIZED
				| gtk.SENSITIVE
				| gtk.PARENT_SENSITIVE
#				| gtk.DOUBLE_BUFFERED
				| gtk.CAN_FOCUS)
		self.window = gdk.Window(
				self.get_parent_window(),
				width = self.allocation.width,
				height = self.allocation.height,
				window_type = gdk.WINDOW_CHILD,
				wclass = gdk.INPUT_OUTPUT,
				event_mask = self.get_events()
						| gdk.EXPOSURE_MASK
						| gdk.KEY_PRESS
						| gdk.KEY_RELEASE
#						| gdk.POINTER_MOTION_MASK
#						| gdk.POINTER_MOTION_HINT_MASK
						| gdk.BUTTON_PRESS_MASK
						| gdk.SCROLL
						| gdk.FOCUS_CHANGE)
		self.window.set_user_data(self)
		self.style.attach(self.window)
		self.style.set_background(self.window, gtk.STATE_NORMAL)
		self.window.move_resize(*self.allocation)
		self.connect('key-press-event', self.handle_key)
		self.connect('key-release-event', self.handle_release)
		self.connect('focus-in-event', self.handle_focus_in)
		self.connect('focus-out-event', self.handle_focus_out)
		self.connect('button-press-event', self.handle_button_press)
		self.connect('scroll-event', self.handle_scroll)
#		self.connect('motion-notify-event', self.handle_motion_notify)

	def do_unrealize(self):
		self.window.set_user_data(None)

	def do_size_request(self, requisition):
		pass

	def do_size_allocate(self, allocation):
		self.allocation = allocation
		if self.flags() & gtk.REALIZED:
			self.window.move_resize(*allocation)

	def do_expose_event(self, event):
		cr = self.window.cairo_create()
		cr.rectangle(event.area.x, event.area.y,
				event.area.width, event.area.height)
		cr.clip()
		self.draw(cr, *self.window.get_size())

	def draw(self, cr, width, height):
		cr.set_source_rgb(*self.ptheme['Background colour'])
		cr.rectangle(0, 0, width, height)
		cr.fill()
		cr.set_line_width(1)

		# Make sure cursor is inside Pattern
		if self.cursor[0] < (0, 0, 0):
			self.cursor = ((0, 0, self.cursor[0][2]), self.cursor[1])
		if self.cursor[1] < 0:
			self.cursor = (self.cursor[0], 0)
		if self.pdata and self.cursor[0][:2] > self.pdata.len:
			self.cursor = (self.pdata.len + (self.cursor[0][2],), self.cursor[1])
		elif not self.pdata:
			self.cursor = ((0, 0, 0), 0)
		if self.cursor[1] > COLUMNS:
			self.cursor = (self.cursor[0], COLUMNS)

		# Make sure cursor is visible
		if self.cursor[1] < self.view_corner[1]:
			self.view_corner = (self.view_corner[0], self.cursor[1])
		if time_sub(self.cursor[0][:2], self.px_time(self.col_font_size)) < self.view_corner[0]:
			self.view_corner = (time_sub(self.cursor[0][:2],
					self.px_time(self.col_font_size)), self.view_corner[1])

		# Calculate space
		col_space = width - self.ruler_width
		col_count = col_space // self.col_width
		col_last = self.view_corner[1] + col_count - 1
		if col_last > COLUMNS:
			diff = col_last - COLUMNS
			self.view_corner = (self.view_corner[0], self.view_corner[1] - diff)
			col_last = COLUMNS
			if self.view_corner[1] < 0:
				col_count += self.view_corner[1]
				self.view_corner = (self.view_corner[0], 0)
		if self.cursor[1] > col_last:
			diff = self.cursor[1] - col_last
			self.view_corner = (self.view_corner[0], self.view_corner[1] + diff)
		beat_space = height - self.col_font_size
		beat_count = divmod(beat_space, self.pixels_per_beat)
		beat_count = (int(beat_count[0]),
				int(beat_count[1] * RELTIME_FULL_PART // self.pixels_per_beat))
		beat_last = time_add(self.view_corner[0], beat_count)
		if self.pdata and beat_last > self.pdata.len:
			diff = time_sub(beat_last, self.pdata.len)
			self.view_corner = (time_sub(self.view_corner[0], diff), self.view_corner[1])
			beat_last = self.pdata.len
		if self.view_corner[0] < time_sub((0, 0), self.px_time(self.col_font_size)):
			beat_count = time_add(beat_count,
					time_sub(self.px_time(self.col_font_size), self.view_corner[0]))
			self.view_corner = (time_sub((0, 0),
					self.px_time(self.col_font_size)), self.view_corner[1])
			if not (self.pdata and beat_last > self.pdata.len):
				beat_last = min(time_add(self.view_corner[0], beat_count), self.pdata.len)
		if self.cursor[0][:2] > beat_last:
			diff = time_sub(self.cursor[0][:2], beat_last)
			self.view_corner = (time_add(self.view_corner[0], diff), self.view_corner[1])
			beat_last = self.cursor[0][:2]

		self.draw_ruler(cr, height, self.view_corner[0], beat_last)

		# Draw columns
		for col_num in range(self.view_corner[1], self.view_corner[1] + col_count):
			self.draw_column(cr, col_num,
					self.ruler_width + (col_num - self.view_corner[1]) * self.col_width,
					0, height, self.view_corner[0], beat_last)

	def time_px(self, time):
		px = float(time[0] * RELTIME_FULL_PART + time[1])
		return px * self.pixels_per_beat / RELTIME_FULL_PART

	def px_time(self, px):
		beats = float(px) / self.pixels_per_beat
		return time_normalise((long(math.floor(beats)),
				int((beats - math.floor(beats)) * RELTIME_FULL_PART)))

	def draw_ruler(self, cr, height, start, end):
		if not self.pdata:
			return
		cr.set_source_rgb(*self.ptheme['Ruler bg colour'])
		cr.rectangle(0, self.col_font_size + max(-self.time_px(start), 0),
				self.ruler_width, min(self.time_px(end), height - self.col_font_size))
		cr.fill()
		cr.set_source_rgb(*self.ptheme['Ruler fg colour'])
		for beat in range(max(start[0], 0), end[0] + 1):
			if not start <= (beat, 0) <= end:
				continue
			pl = self.create_pango_layout('%d' % beat)
			pl.set_font_description(self.ruler_font)
			rw, rh = pl.get_size()
			rw //= pango.SCALE
			rh //= pango.SCALE
			distance = self.time_px(time_sub((beat, 0), start))
			distance = distance + self.col_font_size - (rh / 2)
			cr.move_to(self.ruler_width - rw - 2, distance)
			cr.update_layout(pl)
			cr.show_layout(pl)
		cr.set_source_rgb(*self.ptheme['Border colour'])
		cr.move_to(self.ruler_width - 0.5, 0)
		cr.rel_line_to(0, height)
		cr.stroke()

	def draw_column(self, cr, num, x, y, height, start, end):
		if self.pdata.cols[num]:
			visible = [(k, v) for (k, v) in self.pdata.cols[num].iteritems()
					if start <= k[:2] <= end]
			visible.sort()
			visible_l = [(k, v) for (k, v) in visible if k[2] == 0]
			visible_r = [(k, v) for (k, v) in visible if k[2] != 0]
			prev = None
			offset = 0
			for (k, v) in visible_l:
				rights = [(l, w) for (l, w) in visible_r if l[:2] == k[:2]]
				# Calculate row width
				row_w = self.event_width(v)
				for (l, w) in rights:
					row_w += self.event_width(w)
				# Make sure cursor points to an existing event
				if self.cursor[0][:2] == k[:2]:
					if self.cursor[0][2] < 0:
						self.cursor = self.cursor(self.cursor[0][:2] + (0,), self.cursor[1])
					elif not rights and self.cursor[0][2] > 0:
						self.cursor = (self.cursor[0][:2] + (0,), self.cursor[1])
					elif rights and self.cursor[0][2] > max(rights)[0][2]:
						self.cursor = (self.cursor[0][:2] + (max(rights)[0][2],),
								self.cursor[1])
				off_w = 0
				cur_w = 0
				if self.cursor[0][:2] == k[:2]:
					if self.cursor[0][2] > 0:
						off_w = self.event_width(v)
						for (l, w) in rights:
							if l[2] < self.cursor[0][2]:
								off_w += self.event_width(w)
							elif l[2] == self.cursor[0][2]:
								cur_w = self.event_width(w)
					else:
						cur_w = self.event_width(v)
				# Make sure cursor is visible (if it's located here)
				if self.cursor[0][:2] == k[:2]:
					if off_w < self.event_offset:
						self.event_offset = off_w
					elif off_w + cur_w > self.event_offset + self.col_width:
						self.event_offset = off_w + cur_w - self.col_width
				if self.event_offset + self.col_width > row_w:
					self.event_offset = row_w - self.col_width
				if self.event_offset < 0:
					self.event_offset = 0
				cr.save()
				cr.rectangle(x, 0, self.col_width, height)
				cr.clip()
				py = y + self.col_font_size
				py = self.time_px(time_sub(k[:2], start)) + py
				prev_y = 0
				if prev:
					prev_y = self.time_px(time_sub(prev, start)) + self.col_font_size
				if self.cursor[0][:2] == k[:2]:
					self.draw_row(cr, k[:2], num, [(k, v)] + rights,
							x - self.event_offset, py, prev_y)
				else:
					self.draw_row(cr, k[:2], num, [(k, v)] + rights,
							x, py, prev_y)
				if self.event_offset > 0:
					cr.set_source_rgb(1, 0.2, 0.2)
					cr.move_to(x, py - 1)
					cr.rel_line_to(self.col_font_size / 4, 0)
					cr.rel_line_to(0, -self.col_font_size / 4)
					cr.close_path()
					cr.fill()
				if self.event_offset + self.col_width < row_w:
					cr.set_source_rgb(1, 0.2, 0.2)
					cr.move_to(x + self.col_width, py - 1)
					cr.rel_line_to(-self.col_font_size / 4, 0)
					cr.rel_line_to(0, -self.col_font_size / 4)
					cr.close_path()
					cr.fill()
				cr.restore()
				prev = k[:2]

		if self.cursor[1] == num:
			distance = self.time_px(time_sub(self.cursor[0][:2], start))
			distance = distance + self.col_font_size
			cr.set_line_width(1.8)
			cr.set_source_rgb(*self.ptheme['Cursor colour'])
			cr.move_to(x, distance)
			cr.rel_line_to(self.col_width, 0)
			cr.stroke()
			cr.set_line_width(1)

		cr.set_source_rgb(*self.ptheme['Column header bg colour'])
		cr.rectangle(x, y, self.col_width, self.col_font_size)
		cr.fill()

		cr.set_source_rgb(*self.ptheme['Column header fg colour'])
		pl = None
		if num == 0:
			pl = self.create_pango_layout('Global')
		else:
			pl = self.create_pango_layout('%02d' % num)
		pl.set_font_description(self.col_font)
		cw, ch = pl.get_size()
		cw //= pango.SCALE
		ch //= pango.SCALE
		cr.move_to(x + 0.5 * self.col_width - 0.5 * cw, y)
		cr.update_layout(pl)
		cr.show_layout(pl)

		cr.set_source_rgb(*self.ptheme['Border colour'])
		cr.move_to(x + self.col_width - 0.5, 0)
		cr.rel_line_to(0, height)
		cr.stroke()

	def draw_row(self, cr, pos, col_num, events, x, y, prev_y):
		offset = 0
		for (k, v) in events:
			offset += self.draw_event(cr, k, col_num, v, x + offset, y, prev_y)

	def draw_event(self, cr, pos, col_num, data, x, y, prev_y):
		fg_colour = (1, 0.9, 0.8)
		bg_colour = (0, 0, 0)
		if self.cursor == (pos, col_num):
			tmp = fg_colour
			fg_colour = bg_colour
			bg_colour = tmp
		estr = self.event_str[data[0]](data)
		pl = self.create_pango_layout(estr)
		w, h = pl.get_size()
		w //= pango.SCALE
		h //= pango.SCALE
		if prev_y + h <= y or (self.cursor[0][:2], self.cursor[1]) == (pos[:2], col_num):
			cr.set_source_rgb(*bg_colour)
			cr.rectangle(x, y - h,
					w + (self.col_font_size / 3), h)
			cr.fill()
			cr.set_source_rgb(*fg_colour)
			cr.move_to(x + (self.col_font_size / 6), y - h)
			cr.update_layout(pl)
			cr.show_layout(pl)
		cr.set_source_rgb(1, 0.9, 0.8)
		cr.move_to(x, y)
		cr.rel_line_to(w + (self.col_font_size / 3), 0)
		cr.stroke()
		return w + (self.col_font_size / 3)

	def event_width(self, event):
		etext = self.event_str[event[0]](event)
		pl = self.create_pango_layout(etext)
		pl.set_font_description(self.col_font)
		cw, _ = pl.get_size()
		cw //= pango.SCALE
		return cw + (self.col_font_size / 3)

	def event_str_note_on(self, event):
#		if not self.notes:
		return (str(event[1]) + ',' + str(event[2]) +
				',' + str(event[3]) + ' %02d' % event[4])

	def event_str_note_off(self, event):
		return '==='

	def do_redraw(self):
		if self.window:
			alloc = self.get_allocation()
			rect = gdk.Rectangle(alloc.x, alloc.y,
					alloc.width, alloc.height)
			self.window.invalidate_rect(rect, True)
			self.window.process_updates(True)
		return True

	def __init__(self, engine, server, song_id):
		gtk.Widget.__init__(self)
		self.engine = engine
		self.server = server
		self.song_id = song_id

		self.pdata = None
		self.notes = None

		self.ptheme = {
			'Ruler font': 'Sans 10',
			'Column font': 'Sans 10',
			'Column width': 7,
			'Beat height': 80,
			'Background colour': (0, 0, 0),
			'Border colour': (0.6, 0.6, 0.6),
			'Ruler bg colour': (0, 0.1, 0.2),
			'Ruler fg colour': (0.7, 0.7, 0.7),
			'Column header bg colour': (0, 0.3, 0),
			'Column header fg colour': (0.8, 0.8, 0.8),
			'Cursor colour': (0.7, 0.8, 0.9),
		}

		self.ruler_font = pango.FontDescription(self.ptheme['Ruler font'])
		self.ruler_font_size = self.ruler_font.get_size() // pango.SCALE
		self.ruler_width = self.ruler_font_size * 3
		self.col_font = pango.FontDescription(self.ptheme['Column font'])
		self.col_font_size = int(self.col_font.get_size() * 1.6) // pango.SCALE
		self.col_width = self.ptheme['Column width'] * self.col_font_size

		self.pixels_per_beat = self.ptheme['Beat height']
		# position format is ((beat, part), channel)
		self.view_corner = (time_sub((0, 0), self.px_time(self.col_font_size)), 0)
		self.cursor = ((0, 0, 0), 0)
		self.event_offset = 0
		self.tmove = 0
		self.snap_init_delay = 4
		self.snap_delay = self.snap_init_delay
		self.snap_state = self.snap_delay

		self.event_str = {
			evtype.NOTE_ON: self.event_str_note_on,
			evtype.NOTE_OFF: self.event_str_note_off,
		}


gobject.type_register(Pat_view)


