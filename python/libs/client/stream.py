# Copyright (c) 2026 by the XRootD developers
# This file is part of the XRootD software suite and is distributed under the
# terms of the GNU Lesser General Public License, version 3 or later.
"""Standard Python file objects backed by the synchronous XRootD client."""

import io
import operator

from XRootD.client.file import File
from XRootD.client.flags import OpenFlags
from XRootD.client.responses import XRootDNotFoundError, raise_as_oserror


_CHUNK_SIZE = 4 * 1024 * 1024


def _mode_flags(mode):
    if not isinstance(mode, str):
        raise TypeError('mode must be a string')
    if (not mode or len(set(mode)) != len(mode) or
            any(char not in 'rwxabt+' for char in mode)):
        raise ValueError('invalid file mode: %r' % mode)
    actions = [char for char in 'rwxa' if char in mode]
    if len(actions) != 1 or ('b' in mode and 't' in mode):
        raise ValueError('invalid file mode: %r' % mode)
    action = actions[0]
    flags = {
        'r': OpenFlags.READ,
        'w': OpenFlags.DELETE,
        'x': OpenFlags.NEW,
        'a': OpenFlags.UPDATE,
    }[action]
    if '+' in mode and action == 'r':
        flags = OpenFlags.UPDATE
    return flags, action == 'r' or '+' in mode, action != 'r' or '+' in mode


class RemoteFile(io.RawIOBase):
    """Seekable binary file using the existing XRootD ``File`` binding."""

    def __init__(self, url, mode='rb', timeout=0):
        super().__init__()
        self._file = None
        flags, self._readable, self._writable = _mode_flags(mode)
        self.name = url
        self.mode = mode
        self.timeout = timeout
        self._append = 'a' in mode
        self._position = 0
        self._file = File()
        status, _ = self._file.open(url, flags=flags, timeout=timeout)
        if (self._append and
                isinstance(status.exception(), XRootDNotFoundError)):
            self._file = File()
            status, _ = self._file.open(url, flags=OpenFlags.NEW,
                                        timeout=timeout)
            if (status.code == status.errErrorResponse and
                    status.errno == 3018):
                # Another writer created the file after the first open.
                self._file = File()
                status, _ = self._file.open(url, flags=OpenFlags.UPDATE,
                                            timeout=timeout)
        raise_as_oserror(status, url)
        if self._append:
            try:
                self._position = self._stat_size()
            except Exception:
                self._file.close(timeout=timeout)
                raise

    def _stat_size(self):
        # XrdCl caches the size from open unless forced to refresh. A stale
        # size makes SEEK_END overwrite earlier writes (notably Uproot's
        # header when it extends a newly created ROOT file).
        status, info = self._file.stat(force=True, timeout=self.timeout)
        raise_as_oserror(status, self.name)
        return info.size

    def readable(self):
        self._checkClosed()
        return self._readable

    def writable(self):
        self._checkClosed()
        return self._writable

    def seekable(self):
        self._checkClosed()
        return True

    def tell(self):
        self._checkClosed()
        return self._position

    def seek(self, offset, whence=io.SEEK_SET):
        self._checkClosed()
        offset, whence = operator.index(offset), operator.index(whence)
        if whence == io.SEEK_SET:
            position = offset
        elif whence == io.SEEK_CUR:
            position = self._position + offset
        elif whence == io.SEEK_END:
            position = self._stat_size() + offset
        else:
            raise ValueError('invalid whence: %r' % whence)
        if position < 0:
            raise ValueError('negative seek position')
        self._position = position
        return position

    def readinto(self, buffer):
        self._checkClosed()
        if not self._readable:
            raise io.UnsupportedOperation('file is not readable')
        target = memoryview(buffer).cast('B')
        if target.readonly:
            raise TypeError('readinto() requires a writable buffer')
        total = 0
        while total < len(target):
            size = min(len(target) - total, _CHUNK_SIZE)
            status, data = self._file.read(offset=self._position, size=size,
                                           timeout=self.timeout)
            raise_as_oserror(status, self.name)
            count = len(data)
            target[total:total + count] = data
            self._position += count
            total += count
            if count < size:
                break
        return total

    def write(self, buffer):
        self._checkClosed()
        if not self._writable:
            raise io.UnsupportedOperation('file is not writable')
        source = memoryview(buffer).cast('B')
        if self._append:
            self._position = self._stat_size()
        total = 0
        while total < len(source):
            data = bytes(source[total:total + _CHUNK_SIZE])
            status, _ = self._file.write(data, offset=self._position,
                                         size=len(data), timeout=self.timeout)
            raise_as_oserror(status, self.name)
            count = len(data)
            self._position += count
            total += count
        return total

    def truncate(self, size=None):
        self._checkClosed()
        if not self._writable:
            raise io.UnsupportedOperation('file is not writable')
        if size is None:
            size = self._position
        else:
            size = operator.index(size)
        if size < 0:
            raise ValueError('negative truncate size')
        status, _ = self._file.truncate(size, timeout=self.timeout)
        raise_as_oserror(status, self.name)
        return size

    def flush(self):
        self._checkClosed()
        if (not self.closed and getattr(self, '_writable', False) and
                self._file is not None and self._file.is_open()):
            status, _ = self._file.sync(timeout=self.timeout)
            raise_as_oserror(status, self.name)

    def close(self):
        if self.closed:
            return
        try:
            super().close()
        finally:
            if self._file is not None and self._file.is_open():
                status, _ = self._file.close(timeout=self.timeout)
                raise_as_oserror(status, self.name)


def open(url, mode='rb', buffering=-1, encoding=None, errors=None,
         newline=None, timeout=0):
    """Open an XRootD URL as a standard binary or text file object.

    Read, write, append, exclusive create, and update modes are supported.
    The traditional :class:`XRootD.client.File` status-tuple API remains
    available for callers that need explicit offsets or native callbacks.
    """
    _mode_flags(mode)
    buffering = operator.index(buffering)
    binary = 'b' in mode
    if binary and any(value is not None
                      for value in (encoding, errors, newline)):
        raise ValueError('encoding, errors and newline require text mode')
    if buffering == 0 and not binary:
        raise ValueError('unbuffered text I/O is unsupported')
    if buffering < -1:
        raise ValueError('invalid buffering size')

    raw = RemoteFile(url, mode, timeout)
    if buffering == 0:
        return raw
    size = io.DEFAULT_BUFFER_SIZE if buffering in (-1, 1) else buffering
    try:
        if raw.readable() and raw.writable():
            buffered = io.BufferedRandom(raw, buffer_size=size)
        elif raw.readable():
            buffered = io.BufferedReader(raw, buffer_size=size)
        else:
            buffered = io.BufferedWriter(raw, buffer_size=size)
        if binary:
            return buffered
        return io.TextIOWrapper(buffered, encoding=encoding, errors=errors,
                                newline=newline, line_buffering=buffering == 1)
    except Exception:
        raw.close()
        raise
