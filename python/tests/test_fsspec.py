"""fsspec interoperability through native XRootD callbacks."""

import asyncio
import stat
import uuid
from types import SimpleNamespace

import pytest

fsspec = pytest.importorskip('fsspec')

from XRootD import client  # noqa: E402
from XRootD.client import aio  # noqa: E402
from XRootD.client.fsspec import _ReadHandleCache  # noqa: E402
from XRootD.client.fsspec import XRootDFileSystem  # noqa: E402
from XRootD.client.fsspec import AsyncXRootDFile  # noqa: E402
from XRootD.client.responses import XRootDOperationError  # noqa: E402
from XRootD.client.responses import XRootDStatus  # noqa: E402
from env import SERVER_URL  # noqa: E402


def filesystem():
    return XRootDFileSystem(hostid=client.URL(SERVER_URL).hostid)


@pytest.mark.parametrize('code, error_type', [
    (XRootDStatus.errNotFound, FileNotFoundError),
    (XRootDStatus.errAuthFailed, PermissionError),
])
def test_source_lookup_keeps_open_error(monkeypatch, code, error_type):
    fs = filesystem()
    status = XRootDStatus({'ok': False, 'code': code, 'errno': 0,
                          'message': 'original open failure'})

    class File:
        def open(self, *args, **kwargs):
            return status if kwargs.get('callback') else (status, None)

    async def no_sources(path):
        raise OSError('no allowed source')

    monkeypatch.setattr(client, 'File', File)
    monkeypatch.setattr(fs, '_source_urls', no_sources)
    with pytest.raises(error_type, match='original open failure'):
        fs.open('/missing')

    async def run():
        with pytest.raises(error_type, match='original open failure'):
            await fs.open_async('/missing')

    asyncio.run(run())


def test_update_close_refreshes_listing_and_buffered_read():
    fs = filesystem()
    path = '/tmp/fsspec-update-cache-' + uuid.uuid4().hex
    try:
        fs.pipe_file(path, b'old')
        fs.ls('/tmp')
        assert fs.info(path)['size'] == 3
        with fs.open(path, 'r+b') as file:
            file.seek(0, 2)
            file.write(b'-tail')
        assert fs.info(path)['size'] == 8
        assert next(entry for entry in fs.ls('/tmp', detail=True)
                    if entry['name'] == path)['size'] == 8
        with fs.open(path, 'rb') as file:
            assert file.read() == b'old-tail'
    finally:
        fs.rm_file(path)
        fs.close()


@pytest.mark.parametrize('mode', ['wb', 'ab', 'r+b'])
def test_async_write_close_refreshes_listing(mode):
    async def run():
        fs = XRootDFileSystem(hostid=client.URL(SERVER_URL).hostid,
                              asynchronous=True, skip_instance_cache=True)
        path = '/tmp/fsspec-async-cache-' + uuid.uuid4().hex
        try:
            await fs._pipe_file(path, b'old')
            await fs._ls('/tmp')
            assert (await fs._info(path))['size'] == 3
            async with await fs.open_async(path, mode) as file:
                if mode == 'r+b':
                    await file.seek(0, 2)
                await file.write(b'-tail')
            expected = b'-tail' if mode == 'wb' else b'old-tail'
            assert (await fs._info(path))['size'] == len(expected)
            listing = await fs._ls('/tmp', detail=True)
            assert next(entry for entry in listing
                        if entry['name'] == path)['size'] == len(expected)
            assert await fs._cat_file(path) == expected
        finally:
            await fs._rm_file(path)
            await fs.close_async()

    asyncio.run(run())


def test_async_close_invalidates_even_when_native_close_fails():
    async def run():
        invalidated = []

        class File:
            async def sync(self, timeout):
                pass

            async def close(self, timeout):
                raise OSError('close failed')

        async def invalidate(path):
            invalidated.append(True)

        fs = XRootDFileSystem(hostid='example', asynchronous=True,
                              skip_instance_cache=True)
        fs.invalidate_cache_async = invalidate
        file = AsyncXRootDFile(fs, '/data', 'wb')
        file._file = File()
        file._closed = False
        with pytest.raises(OSError, match='close failed'):
            await file.close()
        assert invalidated == [True]

    asyncio.run(run())


def test_fsspec_explicit_root_registration():
    fsspec.register_implementation('root', XRootDFileSystem, clobber=True)
    assert fsspec.get_filesystem_class('root') is XRootDFileSystem


def test_fsspec_strip_protocol_lists():
    paths = ['root://host.example:1094//data/a?token=1', '/data/b']
    assert XRootDFileSystem._strip_protocol(paths) == \
        ['/data/a?token=1', '/data/b']


def test_fsspec_sync_and_async_operations(tmp_path):
    fs = filesystem()
    path = '/tmp/fsspec-%s' % uuid.uuid4().hex
    copy = path + '-copy'
    local = tmp_path / 'download'
    upload = tmp_path / 'upload'
    upload.write_bytes(b'uploaded')

    try:
        fs.pipe_file(path, b'abcdef')
        assert fs.info(path)['size'] == 6
        assert fs.cat_file(path, start=1, end=4) == b'bcd'
        assert fs.cat_file(path) == b'abcdef'
        assert fs.cat_file(fs.unstrip_protocol(path)) == b'abcdef'
        with fs.open(path, 'rb') as remote:
            assert remote.read() == b'abcdef'
        assert any(item['name'] == path for item in fs.ls('/tmp', detail=True))

        fs.get_file(path, str(local))
        assert local.read_bytes() == b'abcdef'
        fs.put_file(str(upload), copy)
        assert fs.cat_file(copy) == b'uploaded'
        with fs.open(copy, 'wb') as remote:
            remote.write(b'uploaded')
        assert fs.cat_file(copy) == b'uploaded'
        with fs.open(copy, 'ab') as remote:
            remote.write(b' again')
        assert fs.cat_file(copy) == b'uploaded again'

        async def asynchronous():
            async_fs = XRootDFileSystem(
                hostid=client.URL(SERVER_URL).hostid, asynchronous=True)
            async with await async_fs.open_async(path, 'rb') as remote:
                assert await remote.read(3) == b'abc'
                assert await remote.read(3) == b'def'
            assert await async_fs._cat_file(path, 2, 5) == b'cde'
            assert not await async_fs._exists(path + '-missing')
            results = await asyncio.gather(
                async_fs._cat_file(path, 0, 3),
                async_fs._cat_file(path, 3, 6))
            assert results == [b'abc', b'def']
            await async_fs._cp_file(path, copy)
            assert await async_fs._cat_file(copy) == b'abcdef'
            async with await async_fs.open_async(copy, 'wb') as remote:
                await remote.write(b'replaced')
            assert await async_fs._cat_file(copy) == b'replaced'

        asyncio.run(asynchronous())
    finally:
        for name in (path, copy):
            if fs.exists(name):
                fs.rm_file(name)


def test_fsspec_chunked_async_transfer():
    async def run():
        fs = XRootDFileSystem(hostid=client.URL(SERVER_URL).hostid,
                              asynchronous=True)
        path = '/tmp/fsspec-large-%s' % uuid.uuid4().hex
        content = b'x' * (4 * 1024 * 1024 + 17)
        try:
            await fs._pipe_file(path, content)
            async with await fs.open_async(path) as remote:
                assert await remote.read() == content
        finally:
            if await fs._exists(path):
                await fs._rm_file(path)

    asyncio.run(run())


def test_fsspec_download_chunk_size_and_invalidation(tmp_path):
    fs = filesystem()
    path = '/tmp/fsspec-invalidate-%s' % uuid.uuid4().hex
    local = tmp_path / 'download'

    class Callback:
        def __init__(self):
            self.chunks = []

        def relative_update(self, amount):
            self.chunks.append(amount)

    try:
        fs.pipe_file(path, b'abcdefg')
        callback = Callback()
        fs.get_file(path, str(local), chunk_size=3, callback=callback)
        assert local.read_bytes() == b'abcdefg'
        assert callback.chunks == [3, 3, 1]
        assert fs.cat_file(path) == b'abcdefg'
        assert fs.unstrip_protocol(path) in fs._read_handles.handles
        fs.ls('/tmp')
        assert '/tmp' in fs.dircache

        with client.open(SERVER_URL + path, 'wb') as remote:
            remote.write(b'changed')
        fs.invalidate_cache(path)
        assert fs.unstrip_protocol(path) not in fs._read_handles.handles
        assert '/tmp' not in fs.dircache
        assert fs.cat_file(path) == b'changed'

        async def invalidate_from_foreign_loop():
            fs.invalidate_cache(path)
            assert fs.unstrip_protocol(path) not in fs._read_handles.handles

        asyncio.run(invalidate_from_foreign_loop())

        fs.invalidate_cache()
        assert not fs._read_handles.handles

        async def invalidate_async():
            async_fs = XRootDFileSystem(
                hostid=client.URL(SERVER_URL).hostid, asynchronous=True)
            try:
                assert await async_fs._cat_file(path) == b'changed'
                assert async_fs.unstrip_protocol(path) in \
                    async_fs._read_handles.handles
                await async_fs.invalidate_cache_async(path)
                assert async_fs.unstrip_protocol(path) not in \
                    async_fs._read_handles.handles
            finally:
                await async_fs.close_async()

        asyncio.run(invalidate_async())
    finally:
        fs.close()
        if fs.exists(path):
            fs.rm_file(path)


def test_fsspec_checksum_requests_algorithm():
    async def run():
        fs = XRootDFileSystem(hostid=client.URL(SERVER_URL).hostid,
                              asynchronous=True, skip_instance_cache=True)
        queries = []

        async def query(code, path, timeout):
            queries.append(path)
            return b'crc32c deadbeef'

        fs._client = SimpleNamespace(query=query)
        assert await fs._checksum('/file?token=abc&cks.type=md5',
                                  'crc32c') == ('crc32c', 'deadbeef')
        assert queries == ['/file?token=abc&cks.type=crc32c']
        with pytest.raises(OSError, match='Expected md5'):
            await fs._checksum('/file', 'md5')
        with pytest.raises(ValueError, match='checksum algorithm'):
            await fs._checksum('/file', 'md5&bad=1')

    asyncio.run(run())


def test_fsspec_vector_ranges_and_read_handle_invalidation():
    fs = filesystem()
    path = '/tmp/fsspec-vector-%s' % uuid.uuid4().hex
    second = path + '-second'
    try:
        fs.pipe_file(path, b'abcdefghijklmnopqrstuvwxyz')
        fs.pipe_file(second, b'0123456789')
        assert fs.cat_ranges([path, path], [0, 10], [3, 13]) == [
            b'abc', b'klm']

        async def small_limits(file):
            return 2, 3

        fs._vector_limits = small_limits
        paths = [path, path, second, path, path]
        starts = [0, 3, 1, 12, 1]
        ends = [2, 12, 8, 12, 5]
        assert fs.cat_ranges(paths, starts, ends) == [
            b'ab', b'defghijkl', b'1234567', b'', b'bcde']
        assert fs.cat_ranges([path, second], 1, 3) == [b'bc', b'12']
        assert fs.cat_ranges([path] * 5,
                             [None, -4, 24, 100, 5],
                             [None, None, 100, 200, 3]) == [
            b'abcdefghijklmnopqrstuvwxyz', b'wxyz', b'yz', b'', b'']
        assert len(fs._read_handles.handles) == 2
        assert fs.cat_file(path, start=2, end=5) == b'cde'

        fs.pipe_file(path, b'changed')
        assert fs.cat_file(path) == b'changed'
    finally:
        fs.close()
        for name in (path, second):
            if fs.exists(name):
                fs.rm_file(name)


def test_fsspec_metadata_and_file_controls():
    fs = filesystem()
    path = '/tmp/fsspec-controls-%s' % uuid.uuid4().hex
    created = path + '-new'
    try:
        fs.pipe_file(path, b'abcdef')
        fs.chmod(path, 0o640)
        info = fs.info(path)
        assert stat.S_ISREG(info['mode'])
        assert info['mode'] & 0o777 == 0o640
        assert info['size'] == 6
        assert info['mtime'] == fs.modified(path)
        fields = {'uid', 'gid', 'owner', 'group', 'atime', 'ctime', 'ino'}
        assert fields <= info.keys()
        assert fs.ls(path, detail=False) == [path]
        assert path in fs.ls('/tmp', detail=False, force_update=True)
        assert path in fs.ls('/tmp', detail=False)  # cached listing
        with fs.open(fs.ls(path, detail=False)[0], 'rb') as listed:
            assert listed.read(1) == b'a'

        with fs.open(path, 'r+b') as remote:
            remote.seek(2)
            remote.write(b'Z')
            remote.seek(0)
            assert remote.read() == b'abZdef'

        fs.touch(path)
        assert fs.cat_file(path) == b'abZdef'
        fs.touch(path, truncate=True)
        assert fs.cat_file(path) == b''
        fs.touch(created)
        assert fs.info(created)['size'] == 0
        fs.close()

        async def asynchronous():
            async_fs = XRootDFileSystem(
                hostid=client.URL(SERVER_URL).hostid, asynchronous=True)
            try:
                async with await async_fs.open_async(path, 'r+b') as remote:
                    await remote.write(b'hello')
                    assert remote.tell() == 5
                    assert await remote.seek(0, 2) == 5
                    await remote.seek(1)
                    assert await remote.read(3) == b'ell'
            finally:
                await async_fs.close_async()

        asyncio.run(asynchronous())
    finally:
        fs.close()
        for name in (path, created):
            if fs.exists(name):
                fs.rm_file(name)


def test_fsspec_source_fallback_filters_candidates(monkeypatch):
    attempts = []
    failure = XRootDStatus({'ok': False, 'code': 400, 'errno': 3011,
                            'message': 'open failed'})

    class FakeFile:
        async def open(self, url, timeout=0):
            attempts.append(url)
            if 'redirector.example' in url:
                raise XRootDOperationError(failure)
            return self

    async def run():
        fs = XRootDFileSystem(hostid='redirector.example:1094',
                              asynchronous=True,
                              valid_sources=['allowed.example'])

        async def locate(path, flags, timeout):
            return [SimpleNamespace(address='blocked.example:1094'),
                    SimpleNamespace(address='allowed.example:1094')]

        fs._client.locate = locate
        monkeypatch.setattr(aio, 'File', FakeFile)
        url = 'root://redirector.example:1094//data/file'
        assert await fs._source_urls(url) == [
            'root://allowed.example:1094//data/file']
        assert isinstance(await fs._open_read_file(url, 0), FakeFile)

    asyncio.run(run())
    assert attempts == ['root://redirector.example:1094//data/file',
                        'root://allowed.example:1094//data/file']


def test_fsspec_append_creation_and_missing_file_errors():
    fs = filesystem()
    path = '/tmp/fsspec-append-%s' % uuid.uuid4().hex
    missing = path + '-missing'
    try:
        with fs.open(path, 'ab') as remote:
            remote.write(b'first')
        with fs.open(path, 'ab') as remote:
            remote.write(b'-second')
        assert fs.cat_file(path) == b'first-second'
        with fs.open(path, 'rt') as remote:
            assert remote.read() == 'first-second'
        with pytest.raises(FileNotFoundError):
            fs.open(missing, 'rb')
        with pytest.raises(FileNotFoundError):
            fs.cat_file(missing)

        async def asynchronous():
            async_fs = XRootDFileSystem(
                hostid=client.URL(SERVER_URL).hostid, asynchronous=True)
            created = path + '-async'
            try:
                async with await async_fs.open_async(created, 'ab') as remote:
                    await remote.write(b'async')
                async with await async_fs.open_async(created) as remote:
                    assert await remote.read() == b'async'
                with pytest.raises(FileNotFoundError):
                    await async_fs.open_async(missing)
            finally:
                if await async_fs._exists(created):
                    await async_fs._rm_file(created)
                await async_fs.close_async()

        asyncio.run(asynchronous())
    finally:
        fs.close()
        if fs.exists(path):
            fs.rm_file(path)


def test_read_handle_cache_opens_distinct_urls_concurrently():
    async def run():
        opening = set()
        opened = asyncio.Event()
        resume = asyncio.Event()

        class FakeFile:
            async def close(self, timeout):
                pass

        async def open_file(url, timeout):
            opening.add(url)
            if len(opening) == 2:
                opened.set()
            await resume.wait()
            return FakeFile()

        cache = _ReadHandleCache(open_file)
        first = asyncio.create_task(cache.acquire('first', 0))
        second = asyncio.create_task(cache.acquire('second', 0))
        await asyncio.wait_for(opened.wait(), 1)
        resume.set()
        entries = await asyncio.gather(first, second)
        assert opening == {'first', 'second'}
        for entry in entries:
            await cache.release(entry, 0)
        await cache.close(0)

    asyncio.run(run())


def test_read_handle_cache_shares_one_open_for_a_url():
    async def run():
        opened = 0
        resume = asyncio.Event()

        class FakeFile:
            async def close(self, timeout):
                pass

        async def open_file(url, timeout):
            nonlocal opened
            opened += 1
            await resume.wait()
            return FakeFile()

        cache = _ReadHandleCache(open_file)
        first = asyncio.create_task(cache.acquire('same', 0))
        second = asyncio.create_task(cache.acquire('same', 0))
        await asyncio.sleep(0)
        resume.set()
        entries = await asyncio.gather(first, second)
        assert opened == 1
        assert entries[0] is entries[1]
        for entry in entries:
            await cache.release(entry, 0)
        await cache.close(0)

    asyncio.run(run())


def test_read_handle_cache_reopens_after_inflight_invalidation():
    async def run():
        started = asyncio.Event()
        resume = asyncio.Event()
        opened = []

        class FakeFile:
            def __init__(self, number):
                self.number = number
                self.closed = False

            async def close(self, timeout):
                self.closed = True

        async def open_file(url, timeout):
            file = FakeFile(len(opened) + 1)
            opened.append(file)
            if file.number == 1:
                started.set()
                await resume.wait()
            return file

        cache = _ReadHandleCache(open_file)
        request = asyncio.create_task(cache.acquire('same', 0))
        await asyncio.wait_for(started.wait(), 1)
        await cache.invalidate('same', 0)
        resume.set()
        entry = await asyncio.wait_for(request, 1)
        assert opened[0].closed
        assert entry.file is opened[1]
        await cache.release(entry, 0)
        await cache.close(0)

    asyncio.run(run())


def test_read_handle_cache_prunes_idle_handles():
    async def run():
        closed = asyncio.Event()

        class FakeFile:
            async def close(self, timeout):
                closed.set()

        async def open_file(url, timeout):
            return FakeFile()

        cache = _ReadHandleCache(open_file, ttl=0.01)
        entry = await cache.acquire('idle', 0)
        await cache.release(entry, 0)
        await asyncio.wait_for(closed.wait(), 1)
        assert not cache.handles
        await cache.close(0)

    asyncio.run(run())


def test_read_handle_cache_close_waits_for_idle_pruning():
    async def run():
        closing = asyncio.Event()
        finish = asyncio.Event()

        class FakeFile:
            async def close(self, timeout):
                closing.set()
                await finish.wait()

        async def open_file(url, timeout):
            return FakeFile()

        cache = _ReadHandleCache(open_file, ttl=0.01)
        entry = await cache.acquire('idle', 0)
        await cache.release(entry, 0)
        await asyncio.wait_for(closing.wait(), 1)
        close = asyncio.create_task(cache.close(0))
        await asyncio.sleep(0)
        assert not close.done()
        finish.set()
        await asyncio.wait_for(close, 1)

    asyncio.run(run())


def test_fsspec_vector_limits_fall_back_for_older_servers(monkeypatch):
    class FakeClient:
        async def query(self, *args):
            return b'readv_iov_max readv_ior_max'

    class FakeNative:
        def get_property(self, name):
            return 'root://server.example:1094/'

    fs = XRootDFileSystem(hostid='server.example:1094',
                          asynchronous=True)
    monkeypatch.setattr(aio, 'FileSystem', lambda endpoint: FakeClient())
    file = SimpleNamespace(native=FakeNative())
    assert asyncio.run(fs._vector_limits(file)) == (1024, 2097136)


@pytest.mark.parametrize('operation', ['open', 'read'])
def test_cancelled_cached_read_retains_handle(monkeypatch, operation):
    async def run():
        started, resume = asyncio.Event(), asyncio.Event()
        closed = []

        class File:
            async def read(self, offset, size, timeout):
                if operation == 'read':
                    started.set()
                    await resume.wait()
                return b'data'

            async def close(self, timeout):
                closed.append(True)

        async def open_file(url, timeout):
            if operation == 'open':
                started.set()
                await resume.wait()
            return File()

        fs = XRootDFileSystem(hostid='example', asynchronous=True,
                              filehandle_cache_size=0, filehandle_cache_ttl=0,
                              skip_instance_cache=True)
        fs._read_handles.open_file = open_file
        task = asyncio.create_task(fs._cat_file('/data', 0, 4))
        await asyncio.wait_for(started.wait(), 2)
        task.cancel()
        await asyncio.sleep(0)
        task.cancel()
        await asyncio.sleep(0)
        assert not task.done()
        assert not closed
        resume.set()
        with pytest.raises(asyncio.CancelledError):
            await asyncio.wait_for(task, 2)
        assert closed == [True]
        assert not fs._read_handles.handles
        await fs.close_async()

    asyncio.run(run())


def test_vector_failure_drains_other_batches_before_release():
    async def run():
        from types import SimpleNamespace

        started, resume = asyncio.Event(), asyncio.Event()
        closed = []

        class File:
            async def stat(self, force):
                return SimpleNamespace(size=8)

            async def vector_read(self, batch, timeout):
                if batch[0][0] == 0:
                    await started.wait()
                    raise OSError('first batch failed')
                started.set()
                await resume.wait()
                return [SimpleNamespace(offset=4, buffer=b'data')]

            async def close(self, timeout):
                closed.append(True)

        async def open_file(url, timeout):
            return File()

        async def limits(file):
            return 1, 4

        fs = XRootDFileSystem(hostid='example', asynchronous=True,
                              filehandle_cache_size=0, filehandle_cache_ttl=0,
                              skip_instance_cache=True)
        fs._read_handles.open_file = open_file
        fs._vector_limits = limits
        task = asyncio.create_task(fs._vector_read_ranges('/data', [(0, 8)]))
        await asyncio.wait_for(started.wait(), 2)
        for _ in range(5):
            await asyncio.sleep(0)
        assert not task.done()
        assert not closed
        resume.set()
        with pytest.raises(OSError, match='first batch failed'):
            await asyncio.wait_for(task, 2)
        assert closed == [True]
        await fs.close_async()

    asyncio.run(run())
