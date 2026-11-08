# Copyright (c) 2026 by the XRootD developers
# This file is part of the XRootD software suite and is distributed under the
# terms of the GNU Lesser General Public License, version 3 or later.
"""Awaitable XRootD client operations backed by the native callback API.

The existing XRootD client performs the remote work asynchronously. This module
only transfers callback completion to the caller's asyncio event loop; it does
not run synchronous network calls in an executor.
"""

from XRootD.client._asyncio import asyncio
import errno
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from XRootD.client.asyncstream import _OpenContext

from XRootD import client
from XRootD.client.flags import AccessMode, DirListFlags, MkDirFlags, OpenFlags
from XRootD.client.flags import QueryCode, StatInfoFlags
from XRootD.client.responses import XRootDError, raise_as_oserror
from XRootD.client.responses import raise_on_error
from XRootD.client.responses import checksum_query_path, parse_checksum


async def request(method, *args, **kwargs):
    """Await one callback-capable client operation and return its response.

    Cancellation stops waiting but does not cancel an already submitted XrdCl
    operation. The callback keeps the method, arguments, and buffers alive
    until the native operation completes.
    """
    if 'callback' in kwargs:
        raise TypeError('request() manages the callback argument')

    loop = asyncio.get_running_loop()
    future = loop.create_future()
    keepalive = (method, args, kwargs)

    def complete(status, response):
        if future.done():
            return
        try:
            raise_on_error(status)
        except Exception as error:
            future.set_exception(error)
        else:
            future.set_result(response)

    def callback(status, response, hostlist):
        # The native callback runs on an XrdCl thread, not on the event loop.
        # Retain the bound method and buffers even if the waiter is cancelled.
        _ = keepalive
        try:
            loop.call_soon_threadsafe(complete, status, response)
        except RuntimeError:
            # The loop may have closed after cancellation. XrdCl still owns the
            # callback until completion, but there is no waiter to notify.
            pass

    submission = method(*args, callback=callback, **kwargs)
    try:
        raise_on_error(submission)
    except Exception:
        future.cancel()
        raise
    return await future


class File:
    """Awaitable facade over :class:`XRootD.client.File`."""

    def __init__(self, native=None):
        self.native = native if native is not None else client.File()

    async def open(self, url, flags=OpenFlags.READ, mode=AccessMode.NONE,
                   timeout=0):
        await request(self.native.open, url, flags, mode, timeout)
        return self

    async def close(self, timeout=0):
        await request(self.native.close, timeout)

    async def stat(self, force=False, timeout=0):
        return await request(self.native.stat, force, timeout)

    async def read(self, offset, size, timeout=0):
        if size <= 0 or size > 0xffffffff:
            raise ValueError('async reads require a bounded 32-bit size')
        return await request(self.native.read, offset, size, timeout)

    async def write(self, data, offset=0, timeout=0):
        # The native callback form receives a pointer to the input buffer.
        # Own immutable bytes until completion, including after cancellation.
        data = bytes(data)
        if len(data) > 0xffffffff:
            raise ValueError('async writes require a bounded 32-bit size')
        await request(self.native.write, data, offset, len(data), timeout)
        return len(data)

    async def vector_read(self, chunks, timeout=0):
        return await request(self.native.vector_read, chunks, timeout)

    async def sync(self, timeout=0):
        await request(self.native.sync, timeout)

    async def truncate(self, size, timeout=0):
        await request(self.native.truncate, size, timeout)

    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc_value, traceback):
        if self.native.is_open():
            await self.close()


class FileSystem:
    """Awaitable facade over :class:`XRootD.client.FileSystem`."""

    def __init__(self, url, native=None):
        self.native = native if native is not None else client.FileSystem(url)

    async def stat(self, path, timeout=0):
        return await request(self.native.stat, path, timeout)

    async def dirlist(self, path, flags=0, timeout=0):
        return await request(self.native.dirlist, path, flags, timeout)

    async def locate(self, path, flags=0, timeout=0):
        return await request(self.native.locate, path, flags, timeout)

    async def mkdir(self, path, flags=0, mode=0, timeout=0):
        await request(self.native.mkdir, path, flags, mode, timeout)

    async def chmod(self, path, mode, timeout=0):
        await request(self.native.chmod, path, mode, timeout)

    async def truncate(self, path, size, timeout=0):
        await request(self.native.truncate, path, size, timeout)

    async def rmdir(self, path, timeout=0):
        await request(self.native.rmdir, path, timeout)

    async def rm(self, path, timeout=0):
        await request(self.native.rm, path, timeout)

    async def mv(self, source, dest, timeout=0):
        await request(self.native.mv, source, dest, timeout)

    async def query(self, code, arg, timeout=0):
        return await request(self.native.query, code, arg, timeout)

    async def _call(self, operation, path):
        try:
            return await operation
        except XRootDError as error:
            raise_as_oserror(error.status, path)

    async def stat_info(self, path, timeout=0):
        """Return native stat metadata with standard OSError failures."""
        return await self._call(self.stat(path, timeout), path)

    async def _stat_if_exists(self, path, timeout):
        try:
            return await self._call(self.stat(path, timeout), path)
        except FileNotFoundError:
            return None

    async def exists(self, path, timeout=0):
        """Return False only for missing paths; propagate other errors."""
        return await self._stat_if_exists(path, timeout) is not None

    async def is_file(self, path, timeout=0):
        """Return whether the path is a regular file."""
        info = await self._stat_if_exists(path, timeout)
        return bool(info and not info.flags &
                    (StatInfoFlags.IS_DIR | StatInfoFlags.OTHER))

    async def is_dir(self, path, timeout=0):
        """Return whether the path is a directory."""
        info = await self._stat_if_exists(path, timeout)
        return bool(info and info.flags & StatInfoFlags.IS_DIR)

    async def listdir(self, path, timeout=0):
        """Return entry names, like os.listdir."""
        listing = await self._call(self.dirlist(path, timeout=timeout), path)
        return [entry.name for entry in listing]

    async def scandir(self, path, timeout=0):
        """Return DirectoryEntry objects with paths and stat metadata."""
        listing = await self._call(self.dirlist(
            path, DirListFlags.STAT, timeout), path)
        result = []
        for entry in listing:
            item = client.DirectoryEntry(path, entry, entry.statinfo)
            if item.statinfo is None:
                item.statinfo = await self._call(
                    self.stat(item.path, timeout), item.path)
            result.append(item)
        return result

    async def makedirs(self, path, mode=0, exist_ok=False, timeout=0):
        """Create parents and a directory, like os.makedirs."""
        info = await self._stat_if_exists(path, timeout)
        if info is not None:
            if exist_ok and info.flags & StatInfoFlags.IS_DIR:
                return
            raise FileExistsError(errno.EEXIST, 'File exists', path)
        try:
            await self._call(self.mkdir(
                path, MkDirFlags.MAKEPATH, mode, timeout), path)
        except FileExistsError as error:
            if not exist_ok or not await self.is_dir(path, timeout):
                raise error

    async def checksum(self, path, algorithm=None, timeout=0):
        """Return (algorithm, digest), optionally selecting a checksum type."""
        query_path = checksum_query_path(path, algorithm)
        response = await self._call(self.query(
            QueryCode.CHECKSUM, query_path, timeout), path)
        return parse_checksum(response, algorithm)

    async def unlink(self, path, missing_ok=False, timeout=0):
        """Remove one file and optionally ignore a missing path."""
        try:
            await self._call(self.rm(path, timeout), path)
        except FileNotFoundError:
            if not missing_ok:
                raise


def open(url: str, mode: str = 'rb', timeout: int = 0) -> '_OpenContext':
    """Open a binary stream with ``async with aio.open(url) as file``.

    The returned context can also be awaited; the caller then owns the
    stream and must await ``close()``. No network request is submitted until
    the context is entered or awaited. See :class:`AsyncRemoteFile` for the
    cancellation and cursor-sharing contract.
    """
    from XRootD.client.asyncstream import _OpenContext
    return _OpenContext(url, mode, timeout)
