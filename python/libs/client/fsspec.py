# Copyright (c) 2026 by the XRootD developers
# This file is part of the XRootD software suite and is distributed under the
# terms of the GNU Lesser General Public License, version 3 or later.
"""Optional fsspec filesystem backed by XRootD's awaitable client API."""

from XRootD.client._asyncio import asyncio
import io
import posixpath
import stat
import time
from collections.abc import Iterable
from contextlib import asynccontextmanager

from fsspec.asyn import AsyncFileSystem, _run_coros_in_chunks, sync
from fsspec.asyn import sync_wrapper
from fsspec.spec import AbstractBufferedFile

from XRootD import client
from XRootD.client import aio
from XRootD.client.asyncstream import AsyncRemoteFile, _finish, _open_stream
from XRootD.client.flags import DirListFlags, MkDirFlags, OpenFlags, QueryCode
from XRootD.client.flags import StatInfoFlags
from XRootD.client.responses import XRootDError
from XRootD.client.responses import checksum_query_path, parse_checksum
from XRootD.client.responses import raise_as_oserror
from XRootD.client.stream import RemoteFile


_CHUNK_SIZE = 4 * 1024 * 1024
_DEFAULT_VECTOR_CHUNKS = 1024
_DEFAULT_VECTOR_SIZE = 2097136


async def _to_thread(function, *args):
    """Run local file operations off the loop and drain on cancellation."""
    return await _finish(asyncio.to_thread(function, *args))


@asynccontextmanager
async def _local_file(path, mode):
    """Own the result of a threaded open through cancellation and cleanup."""
    file = None

    async def acquire():
        nonlocal file
        file = await _to_thread(open, path, mode)

    try:
        await _finish(acquire())
        yield file
    finally:
        if file is not None:
            await _finish(_to_thread(file.close))


class _CachedHandle:
    def __init__(self, file):
        self.file = file
        self.accessed = time.monotonic()
        self.users = 0
        self.invalid = False


class _ReadHandleCache:
    """Bound idle XrdCl handles without closing a request in progress."""

    def __init__(self, open_file, max_items=256, ttl=30):
        self.open_file = open_file
        self.max_items = max_items
        self.ttl = ttl
        self.handles = {}
        self.pending = {}
        self.generations = {}
        self._lock = None
        self._pruner_task = None
        self._pruner_stop = None
        self._pruner_error = None

    @property
    def lock(self):
        # Use the consuming loop, which may be fsspec's worker loop.
        if self._lock is None:
            self._lock = asyncio.Lock()
        return self._lock

    async def _prune_idle(self, stop):
        try:
            while True:
                try:
                    await asyncio.wait_for(stop.wait(),
                                           timeout=self.ttl)
                except asyncio.TimeoutError:
                    pass
                if stop.is_set():
                    return
                async with self.lock:
                    to_close = self._prune()
                try:
                    await self._close_entries(to_close, 0)
                except Exception as error:
                    # Removed entries cannot poison later reads. Retain the
                    # first failure for explicit filesystem cleanup.
                    if self._pruner_error is None:
                        self._pruner_error = error
        except asyncio.CancelledError:
            pass

    async def acquire(self, url, timeout):
        if self.ttl > 0 and (self._pruner_task is None or
                             self._pruner_task.done()):
            self._pruner_stop = asyncio.Event()
            self._pruner_task = asyncio.create_task(
                self._prune_idle(self._pruner_stop))
        to_close = []
        async with self.lock:
            now = time.monotonic()
            entry = self.handles.get(url)
            if entry is not None and not entry.users and \
                    now - entry.accessed > self.ttl:
                del self.handles[url]
                entry.invalid = True
                to_close.append(entry)
                entry = None
            if entry is not None:
                entry.users += 1
                entry.accessed = now
                to_close.extend(self._prune())
                pending = None
            else:
                pending = self.pending.get(url)
                if pending is None:
                    generation = self.generations.get(url, 0)
                    pending = asyncio.create_task(
                        self._open_entry(url, timeout, generation))
                    self.pending[url] = pending
        if pending is None:
            return await self._return_lease(entry, to_close, timeout)
        await self._close_entries(to_close, timeout)
        await asyncio.shield(pending)
        async with self.lock:
            entry = self.handles.get(url)
            if entry is not None:
                entry.users += 1
                entry.accessed = time.monotonic()
                to_close = self._prune()
            else:
                to_close = []
        if entry is not None:
            return await self._return_lease(entry, to_close, timeout)
        return await self.acquire(url, timeout)

    async def _return_lease(self, entry, to_close, timeout):
        try:
            await self._close_entries(to_close, timeout)
        except BaseException as error:
            try:
                await _finish(self.release(entry, timeout))
            except BaseException:
                pass
            raise error
        return entry

    async def _open_entry(self, url, timeout, generation):
        try:
            file = await self.open_file(url, timeout)
        except BaseException:
            async with self.lock:
                if self.pending.get(url) is asyncio.current_task():
                    del self.pending[url]
            raise
        async with self.lock:
            current = self.pending.get(url) is asyncio.current_task()
            if current:
                del self.pending[url]
            if current and self.generations.get(url, 0) == generation:
                self.handles[url] = _CachedHandle(file)
                return
        await file.close(timeout)

    @staticmethod
    async def _close_entries(entries, timeout):
        error = None
        for entry in entries:
            try:
                await _finish(entry.file.close(timeout))
            except (Exception, asyncio.CancelledError) as failure:
                if error is None or isinstance(
                        failure, asyncio.CancelledError):
                    error = failure
        if error is not None:
            raise error

    async def release(self, entry, timeout):
        async with self.lock:
            entry.users -= 1
            entry.accessed = time.monotonic()
            to_close = [entry] if entry.invalid and not entry.users else []
            to_close.extend(self._prune())
        await self._close_entries(to_close, timeout)

    async def invalidate(self, url, timeout):
        async with self.lock:
            self.generations[url] = self.generations.get(url, 0) + 1
            entry = self.handles.pop(url, None)
            if entry is not None:
                entry.invalid = True
            to_close = [entry] if entry is not None and not entry.users else []
        await self._close_entries(to_close, timeout)

    async def close(self, timeout):
        task, stop = self._pruner_task, self._pruner_stop
        self._pruner_task = None
        pruner_error = None
        if task is not None:
            if not task.done():
                stop.set()
            try:
                await task
            except Exception as error:
                pruner_error = error
        if pruner_error is None:
            pruner_error = self._pruner_error
        self._pruner_error = None
        if self._pruner_stop is stop:
            self._pruner_stop = None
        async with self.lock:
            entries = list(self.handles.values())
            pending = dict(self.pending)
            self.handles.clear()
            for entry in entries:
                entry.invalid = True
            for url in pending:
                self.generations[url] = self.generations.get(url, 0) + 1
            to_close = [entry for entry in entries if not entry.users]
        try:
            await self._close_entries(to_close, timeout)
        finally:
            # Invalidated in-flight opens close their new handle themselves.
            # Keep the loop alive until that cleanup has completed.
            if pending:
                await asyncio.gather(*pending.values(), return_exceptions=True)
        if pruner_error is not None:
            raise pruner_error

    def _prune(self):
        now = time.monotonic()
        idle = sorted((entry.accessed, url, entry)
                      for url, entry in self.handles.items()
                      if not entry.users)
        excess = (max(0, len(self.handles) - self.max_items)
                  if self.max_items is not None else 0)
        to_close = []
        for accessed, url, entry in idle:
            if excess <= 0 and now - accessed <= self.ttl:
                continue
            del self.handles[url]
            entry.invalid = True
            to_close.append(entry)
            excess -= 1
        return to_close

    async def invalidate_all(self, timeout):
        async with self.lock:
            entries = list(self.handles.values())
            self.handles.clear()
            for entry in entries:
                entry.invalid = True
            for url in self.pending:
                self.generations[url] = self.generations.get(url, 0) + 1
            to_close = [entry for entry in entries if not entry.users]
        await self._close_entries(to_close, timeout)


def _file_mode(mode):
    modes = {
        'rb': OpenFlags.READ,
        'wb': OpenFlags.DELETE,
        'xb': OpenFlags.NEW,
        'ab': OpenFlags.UPDATE,
        'r+b': OpenFlags.UPDATE,
    }
    try:
        return modes[mode]
    except KeyError:
        raise ValueError('unsupported XRootD file mode: %s' % mode) from None


def _raise_fsspec_error(error, path):
    if isinstance(error, XRootDError):
        raise_as_oserror(error.status, path)
    raise error


async def _native(operation, path):
    try:
        return await _finish(operation)
    except XRootDError as error:
        _raise_fsspec_error(error, path)


def _identity(name):
    if not name:
        return 0
    try:
        return int(name)
    except (TypeError, ValueError):
        pass
    return 0


def _statinfo_to_info(path, info):
    flags = info.flags
    if flags & StatInfoFlags.IS_DIR:
        kind, file_type = 'directory', stat.S_IFDIR
    elif flags & StatInfoFlags.OTHER:
        kind, file_type = 'other', stat.S_IFLNK
    else:
        kind, file_type = 'file', stat.S_IFREG
    raw_mode = getattr(info, 'mode', None)
    try:
        permissions = int(str(raw_mode), 8) & 0o7777
    except (TypeError, ValueError):
        permissions = (0o444 if flags & StatInfoFlags.IS_READABLE else 0)
        if flags & StatInfoFlags.IS_WRITABLE:
            permissions |= 0o200
        if kind == 'directory' and permissions & 0o444:
            permissions |= 0o111
    mtime = getattr(info, 'mtime', getattr(info, 'modtime', 0))
    result = {
        'name': path, 'size': info.size, 'type': kind,
        'mode': file_type | permissions,
        'mtime': mtime,
        'ctime': getattr(info, 'ctime', mtime),
        'atime': getattr(info, 'atime', mtime),
        'owner': getattr(info, 'owner', ''),
        'group': getattr(info, 'group', ''),
        'nlink': getattr(info, 'nlink', 1),
    }
    result['uid'] = _identity(result['owner'])
    result['gid'] = _identity(result['group'])
    inode = getattr(info, 'id', None)
    if inode is not None:
        try:
            result['ino'] = int(inode)
        except (TypeError, ValueError):
            result['ino'] = inode
    return result


class AsyncXRootDFile(AsyncRemoteFile):
    """Native async stream with fsspec source selection and cache hooks."""

    def __init__(self, fs, path, mode):
        super().__init__(fs.unstrip_protocol(path), mode, fs.timeout)
        self._fs = fs
        self.path = path

    @property
    def loc(self):
        return self.tell()

    async def _initialize(self):
        if self.mode == 'rb':
            self._file = await self._fs._open_read_file(
                self.name, self.timeout)
            self._closed = False
            return self
        await self._fs._invalidate_read_file(self.path)
        return await super()._initialize()

    async def _close(self):
        try:
            await super()._close()
        finally:
            if self._writable:
                await self._fs.invalidate_cache_async(self.path)


class XRootDFile(AbstractBufferedFile):
    """Synchronous buffered file for fsspec's ordinary ``open`` API."""

    def __init__(self, fs, path, mode='rb', **kwargs):
        self._file = client.File()
        remote_path = fs.unstrip_protocol(path)
        status, _ = self._file.open(remote_path, _file_mode(mode),
                                    timeout=fs.timeout)
        if not status.ok and mode == 'ab':
            self._file = client.File()
            status, _ = self._file.open(remote_path, OpenFlags.NEW,
                                        timeout=fs.timeout)
            if not status.ok:
                self._file = client.File()
                status, _ = self._file.open(remote_path, OpenFlags.UPDATE,
                                            timeout=fs.timeout)
        if not status.ok and mode == 'rb' and fs.locate_all_sources:
            original = status
            try:
                sources = sync(fs.loop, fs._source_urls, path)
            except (XRootDError, OSError):
                sources = []
            for source in sources:
                self._file = client.File()
                status, _ = self._file.open(source, OpenFlags.READ,
                                            timeout=fs.timeout)
                if status.ok:
                    break
            if not status.ok:
                status = original
        raise_as_oserror(status, remote_path)
        try:
            append_size = None
            if mode == 'ab':
                append_status, stat = self._file.stat(timeout=fs.timeout)
                raise_as_oserror(append_status, remote_path)
                append_size = stat.size
            super().__init__(fs, path, mode=mode, **kwargs)
            if append_size is not None:
                self.loc = append_size
                self.offset = append_size
        except Exception:
            self._file.close()
            raise

    def _fetch_range(self, start, end):
        status, data = self._file.read(start, end - start,
                                       timeout=self.fs.timeout)
        raise_as_oserror(status, self.path)
        return data

    def _upload_chunk(self, final=False):
        data = self.buffer.getvalue()
        if data:
            status, _ = self._file.write(data, self.offset, len(data),
                                         self.fs.timeout)
            raise_as_oserror(status, self.path)
        return True

    def close(self):
        if self.closed:
            return
        try:
            super().close()
        finally:
            status, _ = self._file.close(timeout=self.fs.timeout)
            raise_as_oserror(status, self.path)


class _UpdateFile(RemoteFile):
    """Invalidate fsspec caches when a standard update-mode stream closes."""

    def __init__(self, fs, path):
        self._fs = fs
        self._path = path
        self._opened = False
        super().__init__(fs.unstrip_protocol(path), 'r+b', fs.timeout)
        self._opened = True

    def close(self):
        if self.closed:
            return
        try:
            super().close()
        finally:
            if self._opened:
                self._fs.invalidate_cache(self._path)


class XRootDFileSystem(AsyncFileSystem):
    """fsspec filesystem for a single XRootD endpoint.

    Construct with ``hostid='host:1094'``. ``asynchronous=True`` allows
    awaiting the underscore-prefixed methods directly. The fsspec dependency
    is optional and is imported only when this module is used.
    """

    protocol = 'root'
    root_marker = '/'
    async_impl = True

    def __init__(self, hostid, timeout=0, locate_all_sources=True,
                 valid_sources=None, **kwargs):
        super().__init__(**kwargs)
        self.hostid = hostid
        self.timeout = timeout
        self.locate_all_sources = locate_all_sources
        self.valid_sources = set(valid_sources or ())
        self._client = aio.FileSystem('root://%s/' % hostid)
        self._read_handles = _ReadHandleCache(
            self._open_read_file,
            max_items=kwargs.get('filehandle_cache_size', 256),
            ttl=kwargs.get('filehandle_cache_ttl', 30))
        self._server_vector_limits = {}
        self._invalidation_tasks = set()
        self._invalidation_error = None

    @asynccontextmanager
    async def _read_file(self, path):
        entry = None

        async def acquire():
            nonlocal entry
            entry = await self._read_handles.acquire(
                self.unstrip_protocol(path), self.timeout)

        try:
            await _finish(acquire())
            yield entry.file
        finally:
            if entry is not None:
                await _finish(self._read_handles.release(entry, self.timeout))

    async def _invalidate_read_file(self, path):
        await self._read_handles.invalidate(self.unstrip_protocol(path),
                                            self.timeout)

    async def _source_urls(self, path):
        locations = await self._client.locate(
            self._path(path), OpenFlags.PREFNAME, self.timeout)
        sources = []
        for location in locations:
            url = client.URL('root://%s/' % location.address)
            if not url.is_valid():
                continue
            if self.valid_sources and url.hostname not in self.valid_sources:
                continue
            candidate = 'root://%s/%s' % (url.hostid, self._path(path))
            if candidate not in sources:
                sources.append(candidate)
        if not sources:
            raise OSError('no allowed XRootD source for %s' % path)
        return sources

    async def _open_read_file(self, url, timeout):
        try:
            return await aio.File().open(url, timeout=timeout)
        except XRootDError as original:
            if not self.locate_all_sources:
                _raise_fsspec_error(original, url)
            try:
                sources = await self._source_urls(url)
            except (XRootDError, OSError):
                _raise_fsspec_error(original, url)
            for source in sources:
                try:
                    return await aio.File().open(source, timeout=timeout)
                except XRootDError:
                    continue
            _raise_fsspec_error(original, url)

    async def close_async(self):
        """Drain scheduled invalidations and close cached read handles.

        Report the first scheduled invalidation failure after cleanup.
        """
        async def close():
            while self._invalidation_tasks:
                # Always yield so done callbacks can collect errors and
                # release tasks, including tasks that already completed.
                await asyncio.wait(self._invalidation_tasks)
            await self._read_handles.close(self.timeout)
            error = self._invalidation_error
            self._invalidation_error = None
            if error is not None:
                raise error

        await _finish(close())

    def close(self):
        """Close cached read handles from synchronous code."""
        if self.asynchronous:
            raise RuntimeError('use await close_async() on async filesystems')
        return sync(self.loop, self.close_async)

    def _invalidate_dircache(self, path):
        if path is None:
            self.dircache.clear()
            return
        path = self._path(path)
        prefix = path.rstrip('/') + '/'
        for cached in list(self.dircache):
            if cached == path or cached.startswith(prefix) or \
                    cached == self._parent(path):
                del self.dircache[cached]

    async def invalidate_cache_async(self, path=None):
        """Discard listings and cached read handles after external changes."""
        super().invalidate_cache(path)
        self._invalidate_dircache(path)
        if path is None:
            await self._read_handles.invalidate_all(self.timeout)
        else:
            await self._read_handles.invalidate(
                self.unstrip_protocol(path), self.timeout)

    def invalidate_cache(self, path=None):
        """Synchronously invalidate caches, or schedule it on a running loop.

        Async callers needing completion should await
        ``invalidate_cache_async``. Scheduled tasks are owned until completion;
        ``close_async`` drains them and reports any deferred failure.
        """
        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:
            loop = None
        if loop is not None and (self.asynchronous or loop is self.loop):
            task = loop.create_task(self.invalidate_cache_async(path))
            self._invalidation_tasks.add(task)
            task.add_done_callback(self._invalidation_done)
            return task
        return sync(self.loop, self.invalidate_cache_async, path)

    def _invalidation_done(self, task):
        self._invalidation_tasks.discard(task)
        if not task.cancelled():
            error = task.exception()
            if error is not None and self._invalidation_error is None:
                self._invalidation_error = error

    async def _vector_limits(self, file):
        server = file.native.get_property('DataServer')
        if not server:
            return _DEFAULT_VECTOR_CHUNKS, _DEFAULT_VECTOR_SIZE
        url = client.URL(server)
        endpoint = '%s://%s/' % (url.protocol, url.hostid)
        if endpoint not in self._server_vector_limits:
            fs = aio.FileSystem(endpoint)
            try:
                response = await fs.query(QueryCode.CONFIG,
                                          'readv_iov_max readv_ior_max',
                                          self.timeout)
                if isinstance(response, bytes):
                    response = response.decode('ascii')
                chunks, size = (int(part) for part in response.split())
                if chunks <= 0 or size <= 0:
                    raise ValueError('invalid XRootD vector-read limits')
            except (XRootDError, ValueError, UnicodeError):
                chunks, size = _DEFAULT_VECTOR_CHUNKS, _DEFAULT_VECTOR_SIZE
            self._server_vector_limits[endpoint] = chunks, size
        return self._server_vector_limits[endpoint]

    @staticmethod
    def _get_kwargs_from_urls(url):
        return {'hostid': client.URL(url).hostid}

    @classmethod
    def _strip_protocol(cls, path):
        if isinstance(path, list):
            return [cls._strip_protocol(item) for item in path]
        if path.startswith('root://'):
            return client.URL(path).path_with_params
        return path

    def _path(self, path):
        if path.startswith('root://'):
            url = client.URL(path)
            if url.hostid != self.hostid:
                raise ValueError('URL belongs to another XRootD host')
            return url.path_with_params
        return path

    def unstrip_protocol(self, path):
        if path.startswith('root://'):
            self._path(path)
            return path
        return 'root://%s/%s' % (self.hostid, path)

    async def _info(self, path, **kwargs):
        path = self._path(path)
        cached = self.dircache.get(self._parent(path))
        if cached is not None and not kwargs.get('force_update'):
            for entry in cached:
                if entry['name'] == path:
                    return entry
        try:
            info = await self._client.stat(path, self.timeout)
        except Exception as error:
            _raise_fsspec_error(error, path)
        return _statinfo_to_info(path, info)

    async def _ls(self, path, detail=True, **kwargs):
        path = self._path(path).rstrip('/') or '/'
        cached = self.dircache.get(path)
        if cached is not None and not kwargs.get('force_update'):
            return cached if detail else [item['name'] for item in cached]
        try:
            listing = await self._client.dirlist(path, DirListFlags.STAT,
                                                 self.timeout)
        except XRootDError as error:
            try:
                info = await self._info(path)
            except FileNotFoundError:
                raise FileNotFoundError(path) from error
            if info['type'] != 'file':
                _raise_fsspec_error(error, path)
            return [info] if detail else [path]
        entries = []
        for entry in listing:
            base, mark, params = path.partition('?')
            full_path = base.rstrip('/') + '/' + entry.name
            if mark:
                full_path += '?' + params
            info = entry.statinfo
            if info is None:
                info = await _native(
                    self._client.stat(full_path, self.timeout), full_path)
            entries.append(_statinfo_to_info(full_path, info))
        self.dircache[path] = entries
        return entries if detail else [entry['name'] for entry in entries]

    async def _mkdir(self, path, create_parents=True, **kwargs):
        flags = MkDirFlags.MAKEPATH if create_parents else MkDirFlags.NONE
        await _native(self._client.mkdir(self._path(path), flags,
                                         timeout=self.timeout), path)
        await self.invalidate_cache_async(self._parent(path))

    async def _makedirs(self, path, exist_ok=False):
        if await self._exists(path):
            if exist_ok and (await self._info(path))['type'] == 'directory':
                return
            raise FileExistsError(path)
        await self._mkdir(path)

    async def _rmdir(self, path):
        await _native(self._client.rmdir(self._path(path), self.timeout), path)
        await self.invalidate_cache_async(self._parent(path))

    rmdir = sync_wrapper(_rmdir)

    async def _rm_file(self, path, **kwargs):
        await self._invalidate_read_file(path)
        await _native(self._client.rm(self._path(path), self.timeout), path)
        await self.invalidate_cache_async(path)
        await self.invalidate_cache_async(self._parent(path))

    async def _rm(self, path, recursive=False, **kwargs):
        paths = await self._expand_path(path, recursive=recursive)
        for item in sorted(paths, key=lambda p: p.count('/'), reverse=True):
            info = await self._info(item)
            if info['type'] == 'directory':
                await self._rmdir(item)
            else:
                await self._rm_file(item)

    async def _mv(self, path1, path2, **kwargs):
        await self._invalidate_read_file(path1)
        await self._invalidate_read_file(path2)
        await _native(self._client.mv(self._path(path1),
                                      self._path(path2), self.timeout), path1)
        await self.invalidate_cache_async(path1)
        await self.invalidate_cache_async(path2)
        await self.invalidate_cache_async(self._parent(path1))
        await self.invalidate_cache_async(self._parent(path2))

    mv = sync_wrapper(_mv)

    async def _touch(self, path, truncate=False, **kwargs):
        path = self._path(path)
        if not truncate and await self._exists(path):
            size = (await self._info(path, force_update=True))['size']
            await _native(self._client.truncate(path, size, self.timeout),
                          path)
        else:
            await self._invalidate_read_file(path)
            async with await self.open_async(path, 'wb'):
                pass
        await self.invalidate_cache_async(path)
        await self.invalidate_cache_async(self._parent(path))

    touch = sync_wrapper(_touch)

    async def _chmod(self, path, mode):
        async def change():
            try:
                await _native(self._client.chmod(self._path(path), mode,
                                                 self.timeout), path)
            finally:
                # Even an uncertain outcome can have changed permissions.
                await self.invalidate_cache_async(path)
                await self.invalidate_cache_async(self._parent(path))

        await _finish(change())

    chmod = sync_wrapper(_chmod)

    async def _modified(self, path):
        return (await self._info(path))['mtime']

    modified = sync_wrapper(_modified)

    async def _checksum(self, path, algorithm='adler32'):
        query_path = checksum_query_path(self._path(path), algorithm)
        response = await _native(self._client.query(
            QueryCode.CHECKSUM, query_path, self.timeout), path)
        return parse_checksum(response, algorithm)

    checksum = sync_wrapper(_checksum)

    async def open_async(self, path, mode='rb', **kwargs) -> AsyncXRootDFile:
        _file_mode(mode)
        return await _open_stream(AsyncXRootDFile(self, path, mode))

    def _open(self, path, mode='rb', block_size=None, **kwargs):
        if mode != 'rb':
            sync(self.loop, self._invalidate_read_file, path)
        if mode == 'r+b':
            raw = _UpdateFile(self, path)
            try:
                return io.BufferedRandom(raw)
            except Exception:
                raw.close()
                raise
        return XRootDFile(self, path, mode=mode, block_size=block_size,
                          **kwargs)

    async def _cat_file(self, path, start=None, end=None, **kwargs):
        async with self._read_file(path) as file:
            if start is None:
                start = 0
            if start < 0 or end is None or end < 0:
                size = (await _native(file.stat(force=True), path)).size
                if start < 0:
                    start = max(0, size + start)
                if end is None:
                    end = size
                elif end < 0:
                    end = max(0, size + end)
            if end <= start:
                return b''
            chunks = []
            remaining = end - start
            while remaining:
                chunk = await _native(
                    file.read(start, min(remaining, _CHUNK_SIZE),
                              self.timeout), path)
                if not chunk:
                    break
                chunks.append(chunk)
                start += len(chunk)
                remaining -= len(chunk)
            return b''.join(chunks)

    async def _vector_read_ranges(self, path, ranges, batch_size=None):
        async with self._read_file(path) as file:
            max_chunks, max_size = await self._vector_limits(file)
            size = (await _native(file.stat(force=True), path)).size
            normalized = []
            for start, end in ranges:
                start = 0 if start is None else start
                end = size if end is None else end
                if start < 0:
                    start = max(0, size + start)
                if end < 0:
                    end = max(0, size + end)
                normalized.append((min(start, size), min(end, size)))
            requests = []
            counts = []
            for start, end in normalized:
                if end <= start:
                    counts.append(0)
                    continue
                count = 0
                while start < end:
                    length = min(end - start, max_size)
                    requests.append((start, length))
                    count += 1
                    start += length
                counts.append(count)
            if not requests:
                return [b'' for _ in ranges]

            batches = [requests[i:i + max_chunks]
                       for i in range(0, len(requests), max_chunks)]
            responses = await _native(_run_coros_in_chunks(
                [file.vector_read(batch, self.timeout) for batch in batches],
                batch_size=batch_size or self.batch_size, nofiles=True,
                return_exceptions=True), path)
            for response in responses:
                if isinstance(response, BaseException):
                    _raise_fsspec_error(response, path)
            chunks = [chunk for response in responses for chunk in response]
            if len(chunks) != len(requests):
                raise OSError('XRootD vector read returned the wrong chunk '
                              'count')
            for (offset, length), chunk in zip(requests, chunks):
                if chunk.offset != offset or len(chunk.buffer) != length:
                    raise OSError('XRootD vector read returned an incomplete '
                                  'range')
            pieces = iter(chunk.buffer for chunk in chunks)
            return [b''.join(next(pieces) for _ in range(count))
                    for count in counts]

    async def _cat_ranges(self, paths, starts, ends, max_gap=None,
                          batch_size=None, on_error='return', **kwargs):
        """Read multiple ranges with XRootD vector reads per remote file."""
        if not isinstance(paths, list):
            raise TypeError('paths must be a list')
        if not isinstance(starts, Iterable):
            starts = [starts] * len(paths)
        if not isinstance(ends, Iterable):
            ends = [ends] * len(paths)
        if not (len(paths) == len(starts) == len(ends)):
            raise ValueError('paths, starts, and ends must have equal lengths')
        if max_gap is not None:
            raise NotImplementedError('max_gap is not supported')
        grouped = {}
        for index, (path, start, end) in enumerate(zip(paths, starts, ends)):
            grouped.setdefault(self._path(path), []).append(
                (index, (start, end)))
        results = [None] * len(paths)

        async def read_one(path, indexed):
            try:
                content = await self._vector_read_ranges(
                    path, [item[1] for item in indexed], batch_size)
            except Exception as error:
                if on_error == 'raise':
                    raise
                content = [error] * len(indexed)
            return [(index, data)
                    for (index, _), data in zip(indexed, content)]

        responses = await _finish(_run_coros_in_chunks(
            [read_one(path, indexed) for path, indexed in grouped.items()],
            batch_size=batch_size or self.batch_size, nofiles=True,
            return_exceptions=True))
        for response in responses:
            if isinstance(response, BaseException):
                raise response
            for index, data in response:
                results[index] = data
        return results

    async def _pipe_file(self, path, value, mode='overwrite', **kwargs):
        if mode not in ('create', 'overwrite'):
            raise ValueError('unsupported write mode: %s' % mode)
        await self._invalidate_read_file(path)
        open_mode = 'xb' if mode == 'create' else 'wb'
        async with await self.open_async(path, open_mode) as file:
            for offset in range(0, len(value), _CHUNK_SIZE):
                await file.write(value[offset:offset + _CHUNK_SIZE])
        await self.invalidate_cache_async(self._parent(path))

    async def _get_file(self, rpath, lpath, callback=None,
                        chunk_size=_CHUNK_SIZE, **kwargs):
        if chunk_size <= 0:
            raise ValueError('chunk_size must be positive')
        async with await self.open_async(rpath) as remote:
            async with _local_file(lpath, 'wb') as local:
                while True:
                    data = await remote.read(chunk_size)
                    if not data:
                        break
                    await _to_thread(local.write, data)
                    if callback is not None:
                        callback.relative_update(len(data))

    async def _put_file(self, lpath, rpath, mode='overwrite',
                        callback=None, **kwargs):
        if mode not in ('create', 'overwrite'):
            raise ValueError('unsupported write mode: %s' % mode)
        async with _local_file(lpath, 'rb') as local:
            await self._invalidate_read_file(rpath)
            open_mode = 'xb' if mode == 'create' else 'wb'
            async with await self.open_async(rpath, open_mode) as remote:
                while True:
                    data = await _to_thread(local.read, _CHUNK_SIZE)
                    if not data:
                        break
                    await remote.write(data)
                    if callback is not None:
                        callback.relative_update(len(data))
        await self.invalidate_cache_async(self._parent(rpath))

    async def _cp_file(self, path1, path2, **kwargs):
        def canonical(path):
            base = self._path(path).partition('?')[0]
            return posixpath.normpath('/' + base.lstrip('/'))

        if canonical(path1) == canonical(path2):
            raise ValueError('source and destination are the same file')
        await self._invalidate_read_file(path2)
        async with await self.open_async(path1) as source:
            async with await self.open_async(path2, 'wb') as target:
                while True:
                    data = await source.read(_CHUNK_SIZE)
                    if not data:
                        break
                    await target.write(data)
        await self.invalidate_cache_async(self._parent(path2))
