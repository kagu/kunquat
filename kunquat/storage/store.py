# -*- coding: utf-8 -*-

#
# Author: Tomi Jylhä-Ollila, Finland 2012
#         Toni Ruottu, Finland 2012
#
# This file is part of Kunquat.
#
# CC0 1.0 Universal, http://creativecommons.org/publicdomain/zero/1.0/
#
# To the extent possible under law, Kunquat Affirmers have waived all
# copyright and related or neighboring rights to Kunquat.
#

import json
import time
import tarfile
import StringIO

SIGNAL_EXPORT_START = 'export_start'
SIGNAL_EXPORT_STATUS = 'export_status'
SIGNAL_EXPORT_END = 'export_end'

class Store(object):
    '''
    >>> from tempfile import mkdtemp
    >>> from shutil import rmtree
    >>> import os
    >>> path = mkdtemp()
    >>> store = Store(path)
    >>> store.flush()
    >>> store['/lol/omg'] = 'jee'
    >>> store['/lol/omg']
    'jee'
    >>> store.commit()
    >>> sid = store._path
    >>> type(sid) == type('')
    True
    >>> os.path.isdir(sid)
    True
    >>> rmtree(path)
    '''

    listeners = []

    def __init__(self, path):
        self._path = path
        self._memory = {}

    def __getitem__(self, key):
        return self._memory[key]

    def __setitem__(self, key, value):
        self._memory[key] = value

    def commit(self):
        pass

    def flush(self):
        pass

    def to_tar(self, path, magic_id, key_prefix=''):
        compression = ''
        if path.endswith('.gz'):
            compression = 'gz'
        elif path.endswith('.bz2'):
            compression = 'bz2'
        self.signal(SIGNAL_EXPORT_START, [len(self._memory)])
        tfile = tarfile.open(path, 'w:' + compression, format=tarfile.USTAR_FORMAT)
        for (key, value) in self._memory.items():
            self.signal(SIGNAL_EXPORT_STATUS, [path, key])
            serial = value if isinstance(value, str) else json.dumps(value)
            data = StringIO.StringIO(serial)
            info = tarfile.TarInfo()
            info.name = magic_id + '/' + key
            info.size = len(serial)
            info.mtime = int(time.mktime(time.localtime(time.time())))
            tfile.addfile(info, fileobj=data)
        tfile.close()
        self.signal(SIGNAL_EXPORT_END)

    def del_tree(self, key_prefix=''):
        pass

    def from_tar(self, path, key_prefix=''):
        pass

    def register_listener(self, listener):
        self.listeners.append(listener)

    def signal(self, etype, eargs=[]):
        e = (etype, eargs)
        for l in self.listeners:
            l.event(e)
        
if __name__ == "__main__":
    import doctest
    doctest.testmod()
