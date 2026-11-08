# Copyright (c) 2026 by the XRootD developers
# This file is part of the XRootD software suite and is distributed under the
# terms of the GNU Lesser General Public License, version 3 or later.
"""Sequential binary streams over XrdCl callbacks, independent of fsspec."""

from XRootD.client._asyncio import asyncio
import io
import operator
from typing import Any, AsyncIterator, Generator, Optional

from XRootD.client import aio
from XRootD.client.flags import OpenFlags
from XRootD.client.responses import XRootDError, raise_as_oserror
from XRootD.client.stream import _CHUNK_SIZE, _mode_flags


async def _finish(operation):
    """Drain a submitted operation before propagating caller cancellation."""
    task = asyncio.ensure_future(operation)
    try:
        return await asyncio.shield(task)
    except asyncio.CancelledError as cancelled:
        # shield alone leaves a background task behind. Keep ownership until
        # completion, including if the caller is cancelled more than once.
        while not task.done():
            try:
                await asyncio.shield(task)
            except asyncio.CancelledError:
                pass
            except Exception:
                break
        try:
            task.result()
        except BaseException:
            pass
        # Preserve the original cancellation through cleanup awaits.
        raise cancelled


async def _open_stream(stream):
    """Acquire a stream, closing a late successful open on cancellation."""
    try:
        await _finish(stream._initialize())
    except BaseException as error:
        try:
            await _finish(stream.close())
        except BaseException:
            # Preserve the opening/cancellation error if cleanup also fails.
            pass
        raise error
    return stream


class AsyncRemoteFile:
    """An awaitable binary file with a sequential cursor and line iteration.

    Use ``aio.open`` to construct an opened stream. I/O and cursor changes
    on one stream are serialized; independent streams can run concurrently.
    Network work uses native callbacks. Cancellation waits for the current
    native request to finish before releasing the stream lock, so closing
    never races that request. A cancelled write can have written bytes and
    advanced the cursor. Use a native ``timeout`` to bound request waits.
    Append mode seeks to the current EOF before each write; it is not an
    atomic append across independent clients.
    """

    def __init__(self, url: str, mode: str, timeout: int):
        self._flags, self._readable, self._writable = _mode_flags(mode)
        if 'b' not in mode:
            raise ValueError('async streams currently require binary mode')
        self.name = url
        self.mode = mode
        self.timeout = timeout
        self._file = aio.File()
        self._lock = asyncio.Lock()
        self._closed = True
        self._position = 0
        self._buffer = b''
        self._positioned_reads = set()

    @property
    def closed(self) -> bool:
        return self._closed

    def _check(self):
        if self.closed:
            raise ValueError('I/O operation on closed file')

    async def _call(self, operation):
        try:
            return await operation
        except XRootDError as error:
            raise_as_oserror(error.status, self.name)

    async def _initialize(self):
        try:
            await self._call(self._file.open(
                self.name, self._flags, timeout=self.timeout))
        except FileNotFoundError:
            if 'a' not in self.mode:
                raise
            self._file = aio.File()
            try:
                await self._call(self._file.open(
                    self.name, OpenFlags.NEW, timeout=self.timeout))
            except FileExistsError:
                self._file = aio.File()
                await self._call(self._file.open(
                    self.name, OpenFlags.UPDATE, timeout=self.timeout))
        self._closed = False
        if 'a' in self.mode:
            self._position = await self._size()
        return self

    async def _size(self):
        return (await self._call(self._file.stat(
            force=True, timeout=self.timeout))).size

    def readable(self) -> bool:
        self._check()
        return self._readable

    def writable(self) -> bool:
        self._check()
        return self._writable

    def seekable(self) -> bool:
        self._check()
        return True

    def tell(self) -> int:
        self._check()
        return self._position

    async def seek(self, offset: int, whence: int = io.SEEK_SET) -> int:
        offset, whence = operator.index(offset), operator.index(whence)
        async with self._lock:
            self._check()
            if whence == io.SEEK_SET:
                position = offset
            elif whence == io.SEEK_CUR:
                position = self._position + offset
            elif whence == io.SEEK_END:
                position = await _finish(self._size()) + offset
            else:
                raise ValueError('invalid whence: %r' % whence)
            if position < 0:
                raise ValueError('negative seek position')
            self._position = position
            self._buffer = b''
            return position

    async def _fill(self, size):
        data = await self._call(self._file.read(
            self._position + len(self._buffer), size, self.timeout))
        self._buffer += data
        return bool(data)

    def _consume(self, size):
        data, self._buffer = self._buffer[:size], self._buffer[size:]
        self._position += len(data)
        return data

    async def _read(self, size, line=False):
        self._check()
        if not self._readable:
            raise io.UnsupportedOperation('file is not readable')
        parts = []
        remaining = size
        while remaining != 0:
            if not self._buffer:
                count = 65536 if line else _CHUNK_SIZE
                if remaining > 0:
                    count = min(count, remaining)
                if not await _finish(self._fill(count)):
                    break
            count = len(self._buffer)
            if remaining > 0:
                count = min(count, remaining)
            end = self._buffer.find(b'\n', 0, count) if line else -1
            if end >= 0:
                count = end + 1
            parts.append(self._consume(count))
            if end >= 0:
                break
            if remaining > 0:
                remaining -= count
        return b''.join(parts)

    async def read(self, size: Optional[int] = -1) -> bytes:
        size = -1 if size is None else operator.index(size)
        async with self._lock:
            return await self._read(size)

    async def _read_at(self, offset, size):
        parts = []
        while size:
            data = await _finish(self._call(self._file.read(
                offset, min(size, _CHUNK_SIZE), self.timeout)))
            if not data:
                break
            parts.append(data)
            offset += len(data)
            size -= len(data)
        return b''.join(parts)

    async def read_at(self, offset: int, size: int) -> bytes:
        """Read up to size bytes without changing the cursor or read buffer.

        Positioned reads on one handle can run concurrently. Close waits for
        them, including cancelled requests. Concurrent writes may affect the
        data returned; this method does not provide snapshot isolation.
        """
        offset, size = operator.index(offset), operator.index(size)
        if offset < 0 or size < 0:
            raise ValueError('read_at requires nonnegative offset and size')
        async with self._lock:
            self._check()
            if not self._readable:
                raise io.UnsupportedOperation('file is not readable')
            task = asyncio.ensure_future(self._read_at(offset, size))
            self._positioned_reads.add(task)
        try:
            return await task
        finally:
            self._positioned_reads.discard(task)

    async def iter_chunks(
            self, size: int = _CHUNK_SIZE) -> AsyncIterator[bytes]:
        """Yield sequential blocks of at most size bytes until EOF.

        Reads are demand-driven: stopping iteration submits no further I/O.
        Use the stream's async context manager to close it on an early exit.
        """
        size = operator.index(size)
        if size <= 0:
            raise ValueError('chunk size must be positive')
        while True:
            block = await self.read(size)
            if not block:
                break
            yield block

    async def readline(self, size: Optional[int] = -1) -> bytes:
        size = -1 if size is None else operator.index(size)
        async with self._lock:
            return await self._read(size, line=True)

    async def readinto(self, buffer: Any) -> int:
        target = memoryview(buffer).cast('B')
        if target.readonly:
            raise TypeError('readinto() requires a writable buffer')
        async with self._lock:
            data = await self._read(len(target))
            target[:len(data)] = data
            return len(data)

    async def _write_chunk(self, data):
        count = await self._call(self._file.write(
            data, self._position, self.timeout))
        self._position += count
        return count

    async def write(self, data: Any) -> int:
        # Own a stable copy before yielding, including mutable buffer inputs.
        data = memoryview(data).tobytes()
        async with self._lock:
            self._check()
            if not self._writable:
                raise io.UnsupportedOperation('file is not writable')
            self._buffer = b''
            if 'a' in self.mode:
                self._position = await _finish(self._size())
            count = 0
            while count < len(data):
                count += await _finish(self._write_chunk(
                    data[count:count + _CHUNK_SIZE]))
            return count

    async def _truncate(self, size):
        await self._call(self._file.truncate(size, self.timeout))
        self._buffer = b''
        return size

    async def truncate(self, size: Optional[int] = None) -> int:
        async with self._lock:
            self._check()
            if not self._writable:
                raise io.UnsupportedOperation('file is not writable')
            size = self._position if size is None else operator.index(size)
            if size < 0:
                raise ValueError('negative truncate size')
            return await _finish(self._truncate(size))

    async def flush(self) -> None:
        async with self._lock:
            self._check()
            if self._writable:
                await _finish(self._call(self._file.sync(self.timeout)))

    async def _close(self):
        try:
            if self._writable:
                await self._call(self._file.sync(self.timeout))
        finally:
            await self._call(self._file.close(self.timeout))
            self._closed = True
            self._buffer = b''

    async def _close_locked(self):
        async with self._lock:
            if not self.closed:
                if self._positioned_reads:
                    await asyncio.gather(*self._positioned_reads,
                                         return_exceptions=True)
                await self._close()

    async def close(self) -> None:
        await _finish(self._close_locked())

    aclose = close

    async def __aenter__(self) -> "AsyncRemoteFile":
        self._check()
        return self

    async def __aexit__(self, exc_type, exc_value, traceback):
        await self.close()

    def __aiter__(self) -> AsyncIterator[bytes]:
        self._check()
        return self

    async def __anext__(self) -> bytes:
        line = await self.readline()
        if not line:
            raise StopAsyncIteration
        return line


class _OpenContext:
    def __init__(self, url: str, mode: str, timeout: int):
        self._args = url, mode, timeout
        self._used = False
        self._stream = None

    async def _open(self) -> AsyncRemoteFile:
        if self._used:
            raise RuntimeError('an aio.open context can only be used once')
        self._used = True
        stream = AsyncRemoteFile(*self._args)
        await _open_stream(stream)
        self._stream = stream
        return stream

    def __await__(self) -> Generator[Any, None, AsyncRemoteFile]:
        return self._open().__await__()

    async def __aenter__(self) -> "AsyncRemoteFile":
        return await self._open()

    async def __aexit__(self, exc_type, exc_value, traceback):
        await self._stream.close()
