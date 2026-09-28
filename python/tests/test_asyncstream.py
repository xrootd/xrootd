"""Python stream contracts and native-request ownership during cancellation."""

import asyncio
import io
import inspect
import uuid

import pytest

from XRootD import client
from XRootD.client import aio
from XRootD.client.responses import XRootDStatus
from env import SERVER_URL


def run_async(coroutine):
    loop = asyncio.new_event_loop()
    try:
        return loop.run_until_complete(coroutine)
    finally:
        loop.close()


@pytest.fixture(params=['native', 'fsspec'])
def open_stream(request):
    if request.param == 'native':
        async def open_file(url, mode='rb'):
            return await aio.open(url, mode)
    else:
        pytest.importorskip('fsspec', minversion='2024.2.0')
        module = pytest.importorskip('XRootD.client.fsspec')
        XRootDFileSystem = module.XRootDFileSystem

        async def open_file(url, mode='rb'):
            fs = XRootDFileSystem(hostid=client.URL(url).hostid,
                                  asynchronous=True, skip_instance_cache=True)
            return await fs.open_async(url, mode)
    return open_file


def test_async_stream_round_trip(monkeypatch):
    path = SERVER_URL + '/tmp/async-stream-' + uuid.uuid4().hex

    async def run():
        def no_executor(*args, **kwargs):
            raise AssertionError('remote I/O must use native callbacks')

        monkeypatch.setattr(asyncio.get_event_loop(), 'run_in_executor',
                            no_executor)
        try:
            async with aio.open(path, 'w+b') as file:
                assert file.readable() and file.writable()
                assert file.seekable()
                data = memoryview(b'first\nsecond\nlast')
                assert await file.write(data) == 17
                await file.flush()
                assert await file.seek(0) == 0
                assert await file.readline(3) == b'fir'
                assert file.tell() == 3
                assert await file.readline() == b'st\n'
                # Writing after read-ahead must use the logical cursor.
                assert await file.write(b'S') == 1
                assert await file.seek(6) == 6
                assert [line async for line in file] == [b'Second\n', b'last']
                assert await file.read(0) == b''
                assert await file.read() == b''
                assert await file.seek(-4, io.SEEK_END) == 13
                target = bytearray(8)
                assert await file.readinto(target) == 4
                assert target[:4] == b'last'
                assert await file.truncate(6) == 6
                assert file.tell() == 17
                await file.seek(0)
                assert await file.read() == b'first\n'
            assert file.closed
            await file.aclose()
            with pytest.raises(ValueError, match='closed'):
                await file.read()

            file = await aio.open(path)
            async with file:
                assert await file.read() == b'first\n'
        finally:
            await aio.FileSystem(SERVER_URL).rm(client.URL(path).path)

    run_async(run())


def test_async_stream_lines_across_buffers():
    path = SERVER_URL + '/tmp/async-stream-' + uuid.uuid4().hex

    async def run():
        lines = [b'\xff' * 65537 + b'\n', b'\n', b'tail']
        try:
            async with aio.open(path, 'wb') as file:
                await file.write(b''.join(lines))
            async with aio.open(path) as file:
                assert [line async for line in file] == lines
        finally:
            await aio.FileSystem(SERVER_URL).rm(client.URL(path).path)

    run_async(run())


def test_cancelled_close_waits_for_a_busy_stream(monkeypatch, open_stream):
    async def run():
        native = DelayedNative('read')
        wrapper = aio.File(native=native)
        monkeypatch.setattr(aio, 'File', lambda: wrapper)
        file = await open_stream('root://example//data')
        read = asyncio.ensure_future(file.read(4))
        await native.started.wait()
        close = asyncio.ensure_future(file.close())
        await asyncio.sleep(0)
        close.cancel()
        await asyncio.sleep(0)
        assert not close.done()
        assert 'close-submitted' not in native.events
        native.release()
        assert await read == b'data'
        with pytest.raises(asyncio.CancelledError):
            await asyncio.wait_for(close, 2)
        assert file.closed

    run_async(run())


def test_async_stream_append_exclusive_and_shared_cursor():
    path = SERVER_URL + '/tmp/async-stream-' + uuid.uuid4().hex

    async def run():
        try:
            async with aio.open(path, 'ab') as file:
                await file.write(b'ab')
                await file.seek(0)
                await file.write(b'cd')
            with pytest.raises(FileExistsError):
                async with aio.open(path, 'xb'):
                    pass
            async with aio.open(path) as file:
                assert await file.read_at(0, 4) == b'abcd'
                # Concurrent requests need not run in argument order.
                chunks = await asyncio.gather(file.read(2), file.read(2))
                assert sorted(chunks) == [b'ab', b'cd']
                assert file.tell() == 4
                with pytest.raises(io.UnsupportedOperation):
                    await file.write(b'x')
                with pytest.raises(TypeError):
                    await file.seek(1.5)
                with pytest.raises(TypeError):
                    await file.readinto(b'immutable')
                assert file.tell() == 4
        finally:
            await aio.FileSystem(SERVER_URL).rm(client.URL(path).path)

    run_async(run())


def test_async_stream_errors_and_context_reuse():
    path = SERVER_URL + '/tmp/async-stream-' + uuid.uuid4().hex

    async def run():
        with pytest.raises(ValueError, match='binary'):
            await aio.open(path, 'w')
        with pytest.raises(FileNotFoundError) as error:
            await aio.open(path)
        assert error.value.filename == path
        assert error.value.xrootd_status is not None
        context = aio.open(path)
        with pytest.raises(FileNotFoundError):
            await context
        with pytest.raises(RuntimeError, match='only be used once'):
            await context

    run_async(run())


class DelayedNative:
    """Callback-driven fake with one manually released native operation."""

    def __init__(self, delayed):
        self.delayed = delayed
        self.started = asyncio.Event()
        self.release = None
        self.opened = False
        self.events = []

    def is_open(self):
        return self.opened

    def submit(self, name, callback, response=None):
        status = XRootDStatus({'ok': True, 'code': 0, 'errno': 0,
                              'message': 'ok'})
        self.events.append(name + '-submitted')

        def complete():
            if name == 'open':
                self.opened = True
            elif name == 'close':
                self.opened = False
            self.events.append(name + '-completed')
            callback(status, response, [])

        if name == self.delayed:
            self.release = complete
            self.started.set()
        else:
            complete()
        return status

    def open(self, *args, **kwargs):
        return self.submit('open', kwargs['callback'])

    def read(self, *args, **kwargs):
        return self.submit('read', kwargs['callback'], b'data')

    def write(self, *args, **kwargs):
        return self.submit('write', kwargs['callback'])

    def sync(self, *args, **kwargs):
        return self.submit('sync', kwargs['callback'])

    def close(self, *args, **kwargs):
        return self.submit('close', kwargs['callback'])


@pytest.mark.parametrize('operation', ['open', 'read', 'write', 'close'])
def test_cancellation_drains_request_before_closing(
        monkeypatch, operation, open_stream):
    async def run():
        native = DelayedNative(operation)
        wrapper = aio.File(native=native)
        monkeypatch.setattr(aio, 'File', lambda: wrapper)
        streams = []

        async def work():
            stream = await open_stream('root://example//data', 'r+b')
            async with stream as file:
                streams.append(file)
                if operation == 'read':
                    await file.read(4)
                elif operation == 'write':
                    await file.write(b'data')

        task = asyncio.ensure_future(work())
        await asyncio.wait_for(native.started.wait(), 2)
        task.cancel()
        await asyncio.sleep(0)
        task.cancel()
        await asyncio.sleep(0)
        assert not task.done()
        if operation != 'close':
            assert 'close-submitted' not in native.events
        native.release()
        with pytest.raises(asyncio.CancelledError):
            await asyncio.wait_for(task, 2)
        assert not native.opened
        assert native.events.count('close-completed') == 1
        if streams:
            assert streams[0].closed

    run_async(run())


def test_stream_matches_local_file_contract(tmpdir, open_stream):
    """Run the same public operations on local and remote files."""
    path = SERVER_URL + '/tmp/stream-contract-' + uuid.uuid4().hex

    async def exercise(file):
        outcomes = []
        operations = [
            ('read', (3,)), ('tell', ()), ('readline', ()),
            ('seek', (-2, io.SEEK_CUR)), ('read', (2,)),
            ('write', (b'XY',)), ('seek', (0,)), ('read', ()),
            ('seek', (-3, io.SEEK_END)), ('readinto', (bytearray(8),)),
            ('read', (0,)), ('read', ()), ('truncate', (4,)),
            ('tell', ()), ('seek', (0,)), ('read', ()),
            ('seek', (-1,)), ('read', (1.5,)),
            ('readinto', (b'immutable',)), ('flush', ()),
            ('close', ()), ('read', ()), ('close', ()),
        ]
        for name, args in operations:
            try:
                result = getattr(file, name)(*args)
                if inspect.isawaitable(result):
                    result = await result
                if name == 'readinto':
                    result = result, bytes(args[0])
                outcomes.append(result)
            except (OSError, ValueError, TypeError) as error:
                # CPython file objects may report EINVAL from the OS;
                # in-memory and remote streams reject the same position
                # with ValueError before submitting an I/O operation.
                if name == 'seek' and args == (-1,):
                    outcomes.append('negative seek rejected')
                else:
                    outcomes.append(type(error))
        return outcomes

    async def run():
        data = b'first\nsecond\nlast'
        from pathlib import Path
        local_path = Path(str(tmpdir)) / 'local'
        local_path.write_bytes(data)
        try:
            with client.open(path, 'wb') as file:
                file.write(data)
            with open(str(local_path), 'r+b') as local:
                expected = await exercise(local)
            async with await open_stream(path, 'r+b') as remote:
                assert await exercise(remote) == expected
            with client.open(path, 'wb') as file:
                file.write(data)
            with client.open(path, 'r+b') as remote:
                assert await exercise(remote) == expected
        finally:
            await aio.FileSystem(SERVER_URL).rm(client.URL(path).path)

    run_async(run())


@pytest.mark.parametrize('deadline', ['wait_for', 'timeout'])
def test_deadline_drains_native_read_before_timeout(
        monkeypatch, open_stream, deadline):
    async def run():
        native = DelayedNative('read')
        wrapper = aio.File(native=native)
        monkeypatch.setattr(aio, 'File', lambda: wrapper)
        async with await open_stream('root://example//data') as file:
            read = asyncio.ensure_future(file.read(4))

            async def wait():
                if deadline == 'wait_for':
                    return await asyncio.wait_for(read, 0.01)
                async with asyncio.timeout(0.01):
                    return await read

            task = asyncio.create_task(wait())
            await asyncio.wait_for(native.started.wait(), 2)
            await asyncio.sleep(0.03)
            assert not read.done()
            assert not task.done()
            assert native.opened
            native.release()
            with pytest.raises(asyncio.TimeoutError):
                await asyncio.wait_for(task, 2)
            with pytest.raises(asyncio.CancelledError):
                await asyncio.wait_for(read, 2)
            assert file.tell() == 0
            # The cancelled request filled the buffer without delivering it.
            assert await file.read(4) == b'data'
            assert native.events.count('read-submitted') == 1

    run_async(run())


def test_cancelled_write_advances_cursor(monkeypatch, open_stream):
    async def run():
        native = DelayedNative('write')
        wrapper = aio.File(native=native)
        monkeypatch.setattr(aio, 'File', lambda: wrapper)
        async with await open_stream('root://example//data', 'r+b') as file:
            task = asyncio.ensure_future(file.write(b'data'))
            await asyncio.wait_for(native.started.wait(), 2)
            task.cancel()
            await asyncio.sleep(0)
            native.release()
            with pytest.raises(asyncio.CancelledError):
                await asyncio.wait_for(task, 2)
            assert file.tell() == 4

    run_async(run())


def test_positioned_reads_and_chunks(open_stream, monkeypatch):
    path = SERVER_URL + '/tmp/stream-positioned-' + uuid.uuid4().hex

    async def run():
        data = b'first\nsecond\nlast'
        try:
            with client.open(path, 'wb') as file:
                file.write(data)
            async with await open_stream(path) as file:
                assert await file.readline() == b'first\n'
                position = file.tell()
                results = await asyncio.gather(
                    file.read_at(0, 5), file.read_at(6, 6),
                    file.read_at(len(data) - 2, 20), file.read_at(100, 3))
                assert results == [b'first', b'second', b'st', b'']
                assert file.tell() == position
                assert await file.readline() == b'second\n'
                await file.seek(0)
                # Force native chunk splitting as well as public iteration.
                from XRootD.client import asyncstream
                monkeypatch.setattr(asyncstream, '_CHUNK_SIZE', 3)
                assert await file.read_at(0, len(data)) == data
                blocks = [part async for part in file.iter_chunks(5)]
                assert blocks == [data[i:i + 5]
                                  for i in range(0, len(data), 5)]
                assert file.tell() == len(data)
                assert await file.read_at(0, 0) == b''
                for offset, size in [(-1, 2), (0, -1)]:
                    with pytest.raises(ValueError):
                        await file.read_at(offset, size)
                with pytest.raises(TypeError):
                    await file.read_at(0.5, 2)
                for size in [0, -1]:
                    with pytest.raises(ValueError):
                        await file.iter_chunks(size).__anext__()
            with pytest.raises(ValueError, match='closed'):
                await file.read_at(0, 1)
        finally:
            await aio.FileSystem(SERVER_URL).rm(client.URL(path).path)

    run_async(run())


def test_positioned_reads_overlap_and_close_drains(monkeypatch, open_stream):
    async def run():
        class Native(DelayedNative):
            def __init__(self):
                super().__init__('read')
                self.pending = {}
                self.both_started = asyncio.Event()

            def read(self, offset, *args, **kwargs):
                status = super().read(offset, *args, **kwargs)
                self.pending[offset] = self.release
                if len(self.pending) == 2:
                    self.both_started.set()
                return status

        native = Native()
        wrapper = aio.File(native=native)
        monkeypatch.setattr(aio, 'File', lambda: wrapper)
        file = await open_stream('root://example//data')
        first = asyncio.ensure_future(file.read_at(0, 4))
        second = asyncio.ensure_future(file.read_at(4, 4))
        await asyncio.wait_for(native.both_started.wait(), 2)
        assert file.tell() == 0
        first.cancel()
        await asyncio.sleep(0)
        first.cancel()
        close = asyncio.ensure_future(file.close())
        await asyncio.sleep(0)
        assert not first.done()
        assert 'close-submitted' not in native.events
        native.pending[0]()
        with pytest.raises(asyncio.CancelledError):
            await asyncio.wait_for(first, 2)
        assert not close.done()
        native.pending[4]()
        assert await asyncio.wait_for(second, 2) == b'data'
        await asyncio.wait_for(close, 2)
        assert file.closed
        assert native.events.count('close-completed') == 1

    run_async(run())


def test_chunk_iteration_does_not_prefetch(monkeypatch, open_stream):
    async def run():
        native = DelayedNative(None)
        wrapper = aio.File(native=native)
        monkeypatch.setattr(aio, 'File', lambda: wrapper)
        async with await open_stream('root://example//data') as file:
            chunks = file.iter_chunks(4)
            assert await chunks.__anext__() == b'data'
            await asyncio.sleep(0)
            assert native.events.count('read-submitted') == 1
            await chunks.aclose()
            assert not file.closed
            assert file.tell() == 4
        assert not native.opened

    run_async(run())


@pytest.mark.parametrize('task_group', [False, True])
def test_async_read_example(task_group, capsys):
    import hashlib
    import importlib.util
    from pathlib import Path
    from types import SimpleNamespace

    source = (Path(__file__).resolve().parents[1] / 'examples' /
              'async_streams' / 'read.py')
    spec = importlib.util.spec_from_file_location(
        'async_read_example', str(source))
    example = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(example)
    path = SERVER_URL + '/tmp/stream-example-' + uuid.uuid4().hex
    data = b'first\nsecond\nlast'

    async def run():
        try:
            with client.open(path, 'wb') as file:
                file.write(data)
            args = SimpleNamespace(url=path, timeout=10, deadline=30,
                                   offset=[0, 6], length=5, chunk_size=3,
                                   task_group=task_group, stream=True)
            assert await example.main(args) == 0
            output = capsys.readouterr().out
            assert 'Sequential cursor: 4' in output
            assert hashlib.sha256(data).hexdigest() in output

            async def native_timeout(args):
                raise TimeoutError('native request expired')

            example.inspect_file = native_timeout
            assert await example.main(args) == 1
            assert 'native request expired' in capsys.readouterr().err
        finally:
            await aio.FileSystem(SERVER_URL).rm(client.URL(path).path)

    run_async(run())


@pytest.mark.parametrize('operation', ['read', 'write', 'seek', 'truncate'])
def test_stream_rejects_unsupported_or_invalid_operations(
        open_stream, operation):
    path = SERVER_URL + '/tmp/stream-rejection-' + uuid.uuid4().hex

    async def run():
        try:
            with client.open(path, 'wb') as file:
                file.write(b'unchanged')
            mode = 'wb' if operation == 'read' else 'rb'
            async with await open_stream(path, mode) as file:
                if operation == 'read':
                    with pytest.raises(io.UnsupportedOperation):
                        await file.read(1)
                    with pytest.raises(io.UnsupportedOperation):
                        await file.read_at(0, 1)
                elif operation == 'write':
                    with pytest.raises(io.UnsupportedOperation):
                        await file.write(b'change')
                    await file.flush()
                elif operation == 'seek':
                    with pytest.raises(ValueError):
                        await file.seek(0, 99)
                else:
                    with pytest.raises(io.UnsupportedOperation):
                        await file.truncate(0)
            async with await open_stream(path, 'r+b') as file:
                with pytest.raises(ValueError):
                    await file.truncate(-1)
        finally:
            await aio.FileSystem(SERVER_URL).rm(client.URL(path).path)

    run_async(run())


def test_cancelled_request_preserves_cancellation_if_callback_fails():
    async def run():
        from XRootD.client.asyncstream import _finish, _open_stream

        entered, release = asyncio.Event(), asyncio.Event()

        async def failing():
            entered.set()
            await release.wait()
            raise OSError('late native failure')

        task = asyncio.ensure_future(_finish(failing()))
        await entered.wait()
        task.cancel()
        await asyncio.sleep(0)
        release.set()
        with pytest.raises(asyncio.CancelledError):
            await asyncio.wait_for(task, 2)

        class Stream:
            async def _initialize(self):
                raise PermissionError('open denied')

            async def close(self):
                raise OSError('cleanup also failed')

        with pytest.raises(PermissionError, match='open denied'):
            await _open_stream(Stream())

    run_async(run())


def test_append_creation_race_retries_on_a_fresh_handle(monkeypatch):
    from types import SimpleNamespace
    from XRootD.client.flags import OpenFlags

    async def run():
        handles, flags = [], []

        class File:
            def __init__(self):
                handles.append(self)

            async def open(self, url, flag, timeout):
                flags.append(flag)
                if len(handles) == 1:
                    raise FileNotFoundError(url)
                if len(handles) == 2:
                    raise FileExistsError(url)
                return self

            async def stat(self, **kwargs):
                return SimpleNamespace(size=3)

            async def sync(self, timeout):
                pass

            async def close(self, timeout):
                pass

        monkeypatch.setattr(aio, 'File', File)
        async with aio.open('root://example//data', 'ab') as stream:
            assert stream.tell() == 3
        assert len(handles) == 3
        assert flags == [OpenFlags.UPDATE, OpenFlags.NEW, OpenFlags.UPDATE]

    run_async(run())
