#-------------------------------------------------------------------------------
# Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
#-------------------------------------------------------------------------------
# This file is part of the XRootD software suite.
#
# XRootD is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# XRootD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
#-------------------------------------------------------------------------------
from __future__ import absolute_import, division, print_function

import asyncio
import errno
import os
import stat

from fsspec.asyn import AsyncFileSystem, _run_coros_in_chunks, sync_wrapper
from fsspec.callbacks import DEFAULT_CALLBACK
from fsspec.spec import AbstractBufferedFile

from XRootD import client
from XRootD.client.flags import AccessMode, DirListFlags, MkDirFlags, OpenFlags
from XRootD.client.flags import QueryCode, StatInfoFlags
from XRootD.client.responses import XRootDStatus

try:
  FileNotFoundError
except NameError:
  FileNotFoundError = IOError

try:
  FileExistsError
except NameError:
  FileExistsError = OSError


DEFAULT_ACCESS_MODE = (AccessMode.UR | AccessMode.UW |
                       AccessMode.GR | AccessMode.GW |
                       AccessMode.OR | AccessMode.OW)

SUPPORTED_PROTOCOLS = ('root', 'xroot', 'roots', 'xroots')


def _normalize_mode(mode):
  mode = mode.replace('t', '')
  if 'b' not in mode:
    mode += 'b'

  if mode in ('rb+', 'br+', '+rb'):
    return 'r+b'
  if mode in ('wb+', 'bw+', '+wb'):
    return 'w+b'
  if mode in ('ab+', 'ba+', '+ab'):
    return 'a+b'
  if mode in ('xb+', 'bx+', '+xb'):
    return 'x+b'
  return mode


def _async_wrap(func):
  async def wrapped(*args, **kwargs):
    loop = asyncio.get_running_loop()
    future = loop.create_future()

    def callback(status, response, hostlist):
      if future.cancelled():
        return
      loop.call_soon_threadsafe(future.set_result, (status, response))

    submit_status = func(*args, callback=callback, **kwargs)
    if not submit_status.ok:
      _raise_if_error(submit_status, 'submit %s' % func.__name__)

    return await future

  return wrapped


def _raise_if_error(status, action, path=None):
  if status.ok:
    return

  message = status.message.strip() if status.message else str(status)
  lower_message = message.lower()
  not_found = (
    getattr(status, 'errno', None) == errno.ENOENT or
    status.code == XRootDStatus.errNotFound or
    'no such file' in lower_message or
    'not found' in lower_message
  )

  if not_found:
    raise FileNotFoundError(message)

  if path is not None:
    message = '%s failed for %s: %s' % (action, path, message)
  else:
    message = '%s failed: %s' % (action, message)
  raise OSError(message)


def _mode_from_flags(flags):
  if flags & StatInfoFlags.IS_DIR:
    file_type = stat.S_IFDIR
    permissions = 0o555 if flags & StatInfoFlags.IS_READABLE else 0
  elif flags & StatInfoFlags.OTHER:
    file_type = stat.S_IFLNK
    permissions = 0
  else:
    file_type = stat.S_IFREG
    permissions = 0o444 if flags & StatInfoFlags.IS_READABLE else 0

  if flags & StatInfoFlags.IS_WRITABLE:
    permissions |= 0o200

  return file_type | permissions


def _info_from_stat(path, stat_info):
  if stat_info.flags & StatInfoFlags.IS_DIR:
    file_type = 'directory'
  elif stat_info.flags & StatInfoFlags.OTHER:
    file_type = 'other'
  else:
    file_type = 'file'

  return {
    'name': path,
    'size': stat_info.size,
    'type': file_type,
    'mtime': stat_info.modtime,
    'mode': _mode_from_flags(stat_info.flags),
    'uid': 0,
    'gid': 0,
    'nlink': 1,
    'atime': stat_info.modtime,
    'ctime': stat_info.modtime,
  }


class XRootDFile(AbstractBufferedFile):
  """fsspec file object backed by :class:`XRootD.client.File`."""

  def __init__(self, fs, path, mode='rb', block_size='default',
               autocommit=True, cache_type='readahead', cache_options=None,
               size=None, **kwargs):
    self._file = None
    self._timeout = kwargs.pop('timeout', fs.timeout)
    self._access_mode = kwargs.pop('access_mode', DEFAULT_ACCESS_MODE)
    mode = _normalize_mode(mode)
    self._random_access = mode in ('r+b', 'w+b', 'a+b', 'x+b')
    if self._random_access:
      self._init_random_access(
        fs, path, mode, block_size, autocommit, size, kwargs
      )
      return

    super(XRootDFile, self).__init__(
      fs, path, mode=mode, block_size=block_size, autocommit=autocommit,
      cache_type=cache_type, cache_options=cache_options, size=size, **kwargs
    )

  @property
  def _url(self):
    return self.fs.unstrip_protocol(self.path)

  def _init_random_access(self, fs, path, mode, block_size, autocommit, size,
                          kwargs):
    self.path = path
    self.fs = fs
    self.mode = mode
    self.blocksize = (
      self.DEFAULT_BLOCK_SIZE if block_size in ('default', None) else block_size
    )
    self.loc = 0
    self.autocommit = autocommit
    self.closed = False
    self.kwargs = kwargs
    self.forced = False
    self.offset = None
    self.size = size

    self._file = client.File()
    flags = OpenFlags.UPDATE
    if mode == 'w+b':
      flags = OpenFlags.DELETE
    elif mode == 'x+b':
      flags = OpenFlags.NEW

    status, _ = self._file.open(self._url, flags, self._access_mode,
                                timeout=self._timeout)
    _raise_if_error(status, 'open', self._url)

    if self.size is None:
      self.size = self.fs.info(self.path)['size']
    if mode == 'a+b':
      self.loc = self.size

  def _open_for_read(self):
    if self._file is not None:
      return

    self._file = client.File()
    status, _ = self._file.open(self._url, OpenFlags.READ,
                                timeout=self._timeout)
    _raise_if_error(status, 'open', self._url)

  def _initiate_upload(self):
    self._file = client.File()
    flags = OpenFlags.UPDATE

    if self.mode == 'wb':
      flags = OpenFlags.DELETE
    elif self.mode == 'xb':
      flags = OpenFlags.NEW
    elif self.mode == 'ab':
      flags = OpenFlags.UPDATE
      self.offset = self.fs.info(self.path)['size']

    status, _ = self._file.open(self._url, flags, self._access_mode,
                                timeout=self._timeout)
    _raise_if_error(status, 'open', self._url)

  def _fetch_range(self, start, end):
    if start >= end:
      return b''

    self._open_for_read()
    status, data = self._file.read(start, end - start, timeout=self._timeout)
    _raise_if_error(status, 'read', self._url)
    return data

  def _upload_chunk(self, final=False):
    data = self.buffer.getvalue()
    if data:
      if not isinstance(data, (bytes, bytearray)):
        data = memoryview(data).tobytes()
      status, _ = self._file.write(data, self.offset, len(data),
                                   timeout=self._timeout)
      _raise_if_error(status, 'write', self._url)

    if final:
      status, _ = self._file.sync(timeout=self._timeout)
      _raise_if_error(status, 'sync', self._url)

  def readable(self):
    if self._random_access:
      return not self.closed
    return super(XRootDFile, self).readable()

  def writable(self):
    if self._random_access:
      return not self.closed
    return super(XRootDFile, self).writable()

  def seekable(self):
    return not self.closed

  def read(self, length=-1):
    if not self._random_access:
      return super(XRootDFile, self).read(length)
    if self.closed:
      raise ValueError('I/O operation on closed file.')
    if length is None or length < 0:
      length = max(0, self.size - self.loc)
    if length == 0:
      return b''

    status, data = self._file.read(self.loc, length, timeout=self._timeout)
    _raise_if_error(status, 'read', self._url)
    self.loc += len(data)
    return data

  def write(self, data):
    if not self._random_access:
      return super(XRootDFile, self).write(data)
    if self.closed:
      raise ValueError('I/O operation on closed file.')
    if not isinstance(data, (bytes, bytearray)):
      data = memoryview(data).tobytes()

    status, _ = self._file.write(data, self.loc, len(data),
                                 timeout=self._timeout)
    _raise_if_error(status, 'write', self._url)
    self.loc += len(data)
    self.size = max(self.size, self.loc)
    return len(data)

  def seek(self, loc, whence=0):
    if not self._random_access:
      return super(XRootDFile, self).seek(loc, whence)
    if self.closed:
      raise ValueError('I/O operation on closed file.')

    loc = int(loc)
    if whence == 0:
      nloc = loc
    elif whence == 1:
      nloc = self.loc + loc
    elif whence == 2:
      nloc = self.size + loc
    else:
      raise ValueError('invalid whence (%s, should be 0, 1 or 2)' % whence)
    if nloc < 0:
      raise ValueError('Seek before start of file')
    self.loc = nloc
    return self.loc

  def flush(self, force=False):
    if not self._random_access:
      return super(XRootDFile, self).flush(force=force)
    if self.closed:
      raise ValueError('Flush on closed file')
    if force:
      self.forced = True

    status, _ = self._file.sync(timeout=self._timeout)
    _raise_if_error(status, 'sync', self._url)

  def close(self):
    try:
      if self._random_access:
        if not self.closed:
          self.flush(force=True)
          self.fs.invalidate_cache(self.path)
          self.fs.invalidate_cache(self.fs._parent(self.path))
          self.closed = True
      else:
        super(XRootDFile, self).close()
    finally:
      if self._file is not None:
        self._file.close(timeout=self._timeout)
        self._file = None


class XRootDFileSystem(AsyncFileSystem):
  """fsspec filesystem implementation for ``root://`` URLs."""

  protocol = SUPPORTED_PROTOCOLS
  root_marker = '/'
  default_timeout = 0
  default_cat_ranges_batch_size = 128

  def __init__(self, hostid=None, protocol='root', xrootd_protocol=None,
               timeout=None,
               **storage_options):
    super(XRootDFileSystem, self).__init__(**storage_options)
    if xrootd_protocol is not None:
      protocol = xrootd_protocol
    if not hostid:
      raise ValueError('XRootDFileSystem requires a hostid')
    if protocol not in SUPPORTED_PROTOCOLS:
      raise ValueError('Unsupported XRootD protocol: %r' % protocol)

    self.hostid = hostid
    self.protocol_name = protocol
    self.timeout = self.default_timeout if timeout is None else timeout
    self.storage_options = storage_options
    self._client = client.FileSystem('%s://%s' % (protocol, hostid))

    if not self._client.url.is_valid():
      raise ValueError('Invalid XRootD hostid: %r' % hostid)

  @staticmethod
  def _get_kwargs_from_urls(urlpath):
    url = client.URL(urlpath)
    return {'hostid': url.hostid, 'xrootd_protocol': url.protocol}

  @classmethod
  def _strip_protocol(cls, path):
    if isinstance(path, (list, tuple)):
      return [cls._strip_protocol(item) for item in path]

    path = os.fspath(path)
    if '://' in path and path.split('://', 1)[0] in SUPPORTED_PROTOCOLS:
      stripped = client.URL(path).path_with_params
    else:
      stripped = path

    stripped = stripped.rstrip('/')
    return stripped or cls.root_marker

  def unstrip_protocol(self, path):
    path = os.fspath(path)
    if '://' in path and path.split('://', 1)[0] in SUPPORTED_PROTOCOLS:
      return path
    path = '/' + path.lstrip('/')
    return '%s://%s/%s' % (self.protocol_name, self.hostid, path)

  def _open(self, path, mode='rb', block_size=None, autocommit=True,
            cache_options=None, **kwargs):
    path = self._strip_protocol(path)
    mode = _normalize_mode(mode)
    return XRootDFile(self, path, mode=mode, block_size=block_size,
                      autocommit=autocommit, cache_options=cache_options,
                      **kwargs)

  def info(self, path, **kwargs):
    path = self._strip_protocol(path)
    status, stat_info = self._client.stat(path, timeout=self.timeout)
    _raise_if_error(status, 'stat', path)
    return _info_from_stat(path, stat_info)

  async def _info(self, path, **kwargs):
    path = self._strip_protocol(path)
    status, stat_info = await _async_wrap(self._client.stat)(
      path, timeout=self.timeout
    )
    _raise_if_error(status, 'stat', path)
    return _info_from_stat(path, stat_info)

  def ls(self, path, detail=True, **kwargs):
    path = self._strip_protocol(path)
    status, dirlist = self._client.dirlist(path, DirListFlags.STAT,
                                           timeout=self.timeout)

    if not status.ok:
      info = self.info(path)
      if info['type'] != 'directory':
        return [info] if detail else [os.path.basename(info['name'])]
      _raise_if_error(status, 'dirlist', path)

    listing = []
    for entry in dirlist:
      entry_path = path.rstrip('/') + '/' + entry.name
      listing.append(_info_from_stat(entry_path, entry.statinfo))

    return listing if detail else [
      os.path.basename(item['name'].rstrip('/')) for item in listing
    ]

  async def _ls(self, path, detail=True, **kwargs):
    path = self._strip_protocol(path)
    status, dirlist = await _async_wrap(self._client.dirlist)(
      path, DirListFlags.STAT, timeout=self.timeout
    )

    if not status.ok:
      info = await self._info(path)
      if info['type'] != 'directory':
        return [info] if detail else [os.path.basename(info['name'])]
      _raise_if_error(status, 'dirlist', path)

    listing = []
    for entry in dirlist:
      entry_path = path.rstrip('/') + '/' + entry.name
      listing.append(_info_from_stat(entry_path, entry.statinfo))

    return listing if detail else [
      os.path.basename(item['name'].rstrip('/')) for item in listing
    ]

  def mkdir(self, path, create_parents=True, **kwargs):
    path = self._strip_protocol(path)
    flags = MkDirFlags.MAKEPATH if create_parents else MkDirFlags.NONE
    status, _ = self._client.mkdir(path, flags=flags, timeout=self.timeout)
    _raise_if_error(status, 'mkdir', path)
    self.invalidate_cache(self._parent(path))

  async def _mkdir(self, path, create_parents=True, **kwargs):
    path = self._strip_protocol(path)
    flags = MkDirFlags.MAKEPATH if create_parents else MkDirFlags.NONE
    status, _ = await _async_wrap(self._client.mkdir)(
      path, flags=flags, timeout=self.timeout
    )
    _raise_if_error(status, 'mkdir', path)
    self.invalidate_cache(self._parent(path))

  def makedirs(self, path, exist_ok=False):
    path = self._strip_protocol(path)
    try:
      info = self.info(path)
    except FileNotFoundError:
      pass
    else:
      if exist_ok and info['type'] == 'directory':
        return
      raise FileExistsError(path)
    self.mkdir(path, create_parents=True)

  async def _makedirs(self, path, exist_ok=False):
    path = self._strip_protocol(path)
    try:
      info = await self._info(path)
    except FileNotFoundError:
      pass
    else:
      if exist_ok and info['type'] == 'directory':
        return
      raise FileExistsError(path)
    await self._mkdir(path, create_parents=True)

  def rm_file(self, path):
    path = self._strip_protocol(path)
    status, _ = self._client.rm(path, timeout=self.timeout)
    _raise_if_error(status, 'rm', path)
    self.invalidate_cache(self._parent(path))

  async def _rm_file(self, path, **kwargs):
    path = self._strip_protocol(path)
    status, _ = await _async_wrap(self._client.rm)(path, timeout=self.timeout)
    _raise_if_error(status, 'rm', path)
    self.invalidate_cache(self._parent(path))

  def rmdir(self, path):
    path = self._strip_protocol(path)
    status, _ = self._client.rmdir(path, timeout=self.timeout)
    _raise_if_error(status, 'rmdir', path)
    self.invalidate_cache(self._parent(path))

  async def _rmdir(self, path):
    path = self._strip_protocol(path)
    status, _ = await _async_wrap(self._client.rmdir)(
      path, timeout=self.timeout
    )
    _raise_if_error(status, 'rmdir', path)
    self.invalidate_cache(self._parent(path))

  def mv(self, path1, path2, recursive=False, maxdepth=None, **kwargs):
    path1 = self._strip_protocol(path1)
    path2 = self._strip_protocol(path2)
    status, _ = self._client.mv(path1, path2, timeout=self.timeout)
    _raise_if_error(status, 'mv', path1)
    self.invalidate_cache(self._parent(path1))
    self.invalidate_cache(self._parent(path2))

  async def _mv_file(self, path1, path2):
    path1 = self._strip_protocol(path1)
    path2 = self._strip_protocol(path2)
    status, _ = await _async_wrap(self._client.mv)(
      path1, path2, timeout=self.timeout
    )
    _raise_if_error(status, 'mv', path1)
    self.invalidate_cache(self._parent(path1))
    self.invalidate_cache(self._parent(path2))

  def cp_file(self, path1, path2, **kwargs):
    stripped_path2 = self._strip_protocol(path2)
    path1 = self.unstrip_protocol(self._strip_protocol(path1))
    path2 = self.unstrip_protocol(stripped_path2)
    status, _ = self._client.copy(path1, path2, force=kwargs.get('force', False))
    _raise_if_error(status, 'copy', path1)
    self.invalidate_cache(self._parent(stripped_path2))

  async def _cp_file(self, path1, path2, **kwargs):
    stripped_path2 = self._strip_protocol(path2)
    path1 = self.unstrip_protocol(self._strip_protocol(path1))
    path2 = self.unstrip_protocol(stripped_path2)
    loop = asyncio.get_running_loop()
    status, _ = await loop.run_in_executor(
      None, self._client.copy, path1, path2, kwargs.get('force', False)
    )
    _raise_if_error(status, 'copy', path1)
    self.invalidate_cache(self._parent(stripped_path2))

  def cat_file(self, path, start=None, end=None, **kwargs):
    path = self._strip_protocol(path)
    if start is not None and start < 0:
      start = self.info(path)['size'] + start
    if end is not None and end < 0:
      end = self.info(path)['size'] + end

    offset = start or 0
    size = 0 if end is None else end - offset
    if size < 0:
      return b''

    file_obj = client.File()
    url = self.unstrip_protocol(path)
    status, _ = file_obj.open(url, OpenFlags.READ, timeout=self.timeout)
    _raise_if_error(status, 'open', url)
    try:
      status, data = file_obj.read(offset, size, timeout=self.timeout)
      _raise_if_error(status, 'read', url)
      return data
    finally:
      file_obj.close(timeout=self.timeout)

  async def _cat_file(self, path, start=None, end=None, **kwargs):
    path = self._strip_protocol(path)
    if start is not None and start < 0:
      start = (await self._info(path))['size'] + start
    if end is not None and end < 0:
      end = (await self._info(path))['size'] + end

    offset = start or 0
    size = 0 if end is None else end - offset
    if size < 0:
      return b''

    file_obj = client.File()
    url = self.unstrip_protocol(path)
    status, _ = await _async_wrap(file_obj.open)(
      url, OpenFlags.READ, timeout=self.timeout
    )
    _raise_if_error(status, 'open', url)
    try:
      status, data = await _async_wrap(file_obj.read)(
        offset, size, timeout=self.timeout
      )
      _raise_if_error(status, 'read', url)
      return data
    finally:
      await _async_wrap(file_obj.close)(timeout=self.timeout)

  async def _get_file(self, rpath, lpath, callback=DEFAULT_CALLBACK, **kwargs):
    data = await self._cat_file(rpath, **kwargs)
    callback.set_size(len(data))
    with open(lpath, 'wb') as file_obj:
      file_obj.write(data)
    callback.relative_update(len(data))

  async def _cat_ranges(self, paths, starts, ends, max_gap=None,
                        batch_size=None, on_error='return', **kwargs):
    if max_gap is not None:
      raise NotImplementedError
    if not isinstance(paths, list):
      raise TypeError
    if not isinstance(starts, list):
      starts = [starts] * len(paths)
    if not isinstance(ends, list):
      ends = [ends] * len(paths)
    if len(starts) != len(paths) or len(ends) != len(paths):
      raise ValueError

    grouped = {}
    for index, (path, start, end) in enumerate(zip(paths, starts, ends)):
      path = self._strip_protocol(path)
      grouped.setdefault(path, []).append((index, start, end))

    batch_size = batch_size or self.batch_size
    batch_size = batch_size or self.default_cat_ranges_batch_size
    coros = [
      self._cat_vector_read(path, ranges, on_error)
      for path, ranges in grouped.items()
    ]
    results = await _run_coros_in_chunks(
      coros, batch_size=batch_size, nofiles=True
    )

    out = [None] * len(paths)
    for path_results in results:
      for index, data in path_results:
        out[index] = data
    return out

  cat_ranges = sync_wrapper(_cat_ranges)

  async def _cat_vector_read(self, path, ranges, on_error):
    file_obj = client.File()
    url = self.unstrip_protocol(path)
    try:
      status, _ = await _async_wrap(file_obj.open)(
        url, OpenFlags.READ, timeout=self.timeout
      )
      _raise_if_error(status, 'open', url)

      vectors = []
      for _, start, end in ranges:
        offset = start or 0
        size = 0 if end is None else end - offset
        if size < 0:
          vectors.append((offset, 0))
        else:
          vectors.append((offset, size))

      status, buffers = await _async_wrap(file_obj.vector_read)(
        vectors, self.timeout
      )
      _raise_if_error(status, 'vector_read', url)

      out = []
      for (index, _, _), buffer in zip(ranges, buffers):
        out.append((index, buffer.buffer))
      return out
    except Exception as exc:
      if on_error == 'return':
        return [(index, exc) for index, _, _ in ranges]
      raise
    finally:
      try:
        await _async_wrap(file_obj.close)(timeout=self.timeout)
      except Exception:
        if on_error != 'return':
          raise

  async def _modified(self, path):
    return (await self._info(path))['mtime']

  modified = sync_wrapper(_modified)

  async def _checksum(self, path, algorithm='adler32'):
    path = self._strip_protocol(path)
    status, response = await _async_wrap(self._client.query)(
      QueryCode.CHECKSUM, path, timeout=self.timeout
    )
    _raise_if_error(status, 'checksum', path)
    text = response.decode() if isinstance(response, bytes) else response
    parts = text.strip('\x00').strip().split()
    if len(parts) < 2:
      raise OSError('Unexpected checksum response: %r' % text)
    return parts[0], parts[1]

  checksum = sync_wrapper(_checksum)

  def touch(self, path, truncate=True, **kwargs):
    if truncate or not self.exists(path):
      with self.open(path, 'wb', **kwargs):
        pass
    else:
      raise NotImplementedError('XRootD does not support updating mtime only')

  def chmod(self, path, mode):
    path = self._strip_protocol(path)
    status, _ = self._client.chmod(path, mode, timeout=self.timeout)
    _raise_if_error(status, 'chmod', path)

  async def _chmod(self, path, mode):
    path = self._strip_protocol(path)
    status, _ = await _async_wrap(self._client.chmod)(
      path, mode, timeout=self.timeout
    )
    _raise_if_error(status, 'chmod', path)
