"""Filesystem contracts, malformed responses, and native handle ownership."""

import asyncio
import gc
import stat
import uuid
import weakref
from types import SimpleNamespace

import pytest

pytest.importorskip('fsspec', minversion='2024.2.0')

from XRootD import client  # noqa: E402
from XRootD.client import aio  # noqa: E402
from XRootD.client.flags import StatInfoFlags  # noqa: E402
from XRootD.client.fsspec import XRootDFileSystem  # noqa: E402
from XRootD.client.fsspec import _ReadHandleCache  # noqa: E402
from XRootD.client.fsspec import _statinfo_to_info  # noqa: E402
from env import SERVER_URL  # noqa: E402


@pytest.fixture
def fs():
    filesystem = XRootDFileSystem(hostid=client.URL(SERVER_URL).hostid,
                                  skip_instance_cache=True)
    try:
        yield filesystem
    finally:
        filesystem.close()


@pytest.mark.parametrize('detail', [False, True])
def test_listing_fetches_missing_metadata(fs, monkeypatch, detail):
    info = SimpleNamespace(flags=0, size=7, modtime=1, id='id')
    calls = []

    async def dirlist(path, flags, timeout):
        return [SimpleNamespace(name='present', statinfo=info),
                SimpleNamespace(name='missing', statinfo=None)]

    async def stat(path, timeout):
        calls.append((path, timeout))
        return info

    monkeypatch.setattr(fs._client, 'dirlist', dirlist)
    monkeypatch.setattr(fs._client, 'stat', stat)

    async def run():
        path = '/data?token=value'
        result = await fs._ls(path, detail=detail)
        expected = ['/data/present?token=value', '/data/missing?token=value']
        assert ([(item['name']) for item in result] if detail else result) == \
            expected
        if detail:
            assert [item['size'] for item in result] == [7, 7]
        assert calls == [('/data/missing?token=value', fs.timeout)]
        assert await fs._ls(path, detail=detail) == result
        assert len(calls) == 1

    asyncio.run(run())


def test_listing_metadata_failure_does_not_cache_partial_results(
        fs, monkeypatch):
    from XRootD.client.responses import XRootDError, XRootDStatus

    info = SimpleNamespace(flags=0, size=7, modtime=1, id='id')
    missing = XRootDStatus(dict(ok=False, code=XRootDStatus.errNotFound,
                                errno=0, message='missing'))

    async def dirlist(path, flags, timeout):
        return [SimpleNamespace(name='present', statinfo=info),
                SimpleNamespace(name='missing', statinfo=None)]

    async def stat(path, timeout):
        raise XRootDError(missing)

    monkeypatch.setattr(fs._client, 'dirlist', dirlist)
    monkeypatch.setattr(fs._client, 'stat', stat)

    async def run():
        with pytest.raises(FileNotFoundError) as caught:
            await fs._ls('/data?token=value')
        assert caught.value.filename == '/data/missing?token=value'
        assert '/data?token=value' not in fs.dircache

    asyncio.run(run())


def test_directory_operations_refresh_cached_metadata(fs):
    root = '/tmp/fsspec-tree-' + uuid.uuid4().hex
    fs.makedirs(root + '/a/b')
    try:
        fs.makedirs(root, exist_ok=True)
        with pytest.raises(FileExistsError):
            fs.makedirs(root)
        fs.mkdir(root + '/empty', create_parents=False)
        fs.pipe_file(root + '/a/file', b'content')
        assert sorted(fs.ls(root + '/a', detail=False)) == [
            root + '/a/b', root + '/a/file']
        assert fs.info(root + '/a')['type'] == 'directory'
        with pytest.raises(FileExistsError):
            fs.makedirs(root + '/a/file', exist_ok=True)
        fs.mv(root + '/a/file', root + '/a/moved')
        assert not fs.exists(root + '/a/file')
        assert fs.cat_file(root + '/a/moved') == b'content'
        fs.rmdir(root + '/empty')
        assert root + '/empty' not in fs.ls(root, detail=False)
        with pytest.raises(FileNotFoundError):
            fs.ls(root + '/missing')
        # Listings with CGI parameters must preserve them on returned paths.
        listing = fs.ls(root + '/a?opaque=value', detail=False)
        assert set(listing) == {root + '/a/b?opaque=value',
                                root + '/a/moved?opaque=value'}
    finally:
        fs.rm(root, recursive=True)
    assert not fs.exists(root)


def test_touch_preserves_content_after_another_writer_extends_file(fs):
    root = '/tmp/fsspec-touch-' + uuid.uuid4().hex
    path = root + '/data'
    fs.pipe_file(path, b'old')
    try:
        fs.ls(root)
        # Bypass this filesystem's cache invalidation, as an external writer
        # would, leaving the cached listing with the original size.
        with client.open(SERVER_URL + path, 'ab') as writer:
            writer.write(b'-extended')
        assert fs.info(path)['size'] == 3
        fs.touch(path, truncate=False)
        with client.open(SERVER_URL + path, 'rb') as reader:
            assert reader.read() == b'old-extended'
    finally:
        fs.rm(root, recursive=True)


@pytest.mark.parametrize('alias', ['relative', 'slashes', 'dot', 'query',
                                   'url-relative', 'url-absolute'])
def test_copy_rejects_aliases_of_source_before_opening(fs, monkeypatch, alias):
    path = '/tmp/fsspec-copy-' + uuid.uuid4().hex
    aliases = {
        'relative': path.lstrip('/'),
        'slashes': path.replace('/tmp/', '/tmp//'),
        'dot': path.replace('/tmp/', '/tmp/child/../'),
        'query': path + '?opaque=value',
        'url-relative': SERVER_URL + path.lstrip('/'),
        'url-absolute': SERVER_URL + path,
    }
    fs.pipe_file(path, b'preserve source')

    async def unexpected_open(*args, **kwargs):
        raise RuntimeError('same-file copy reached open_async')

    monkeypatch.setattr(fs, 'open_async', unexpected_open)
    try:
        with pytest.raises(ValueError, match='same file'):
            fs.cp_file(path, aliases[alias])
        with client.open(SERVER_URL + path, 'rb') as reader:
            assert reader.read() == b'preserve source'
    finally:
        fs.rm_file(path)


@pytest.mark.parametrize('fails', [False, True])
def test_cancelled_chmod_invalidates_caches_after_native_completion(
        monkeypatch, fails):
    async def run():
        started, release = asyncio.Event(), asyncio.Event()
        fs = XRootDFileSystem(hostid='example', asynchronous=True,
                              skip_instance_cache=True)
        events = []
        fs.dircache['/tmp'] = [{'name': '/tmp/data', 'size': 3}]

        async def chmod(path, mode, timeout):
            started.set()
            await release.wait()
            events.append('completed')
            if fails:
                raise OSError('uncertain remote outcome')

        async def invalidate(url, timeout):
            events.append(url)

        monkeypatch.setattr(fs._client, 'chmod', chmod)
        monkeypatch.setattr(fs._read_handles, 'invalidate', invalidate)
        task = asyncio.create_task(fs._chmod('/tmp/data', 0o600))
        try:
            await asyncio.wait_for(started.wait(), 2)
            task.cancel()
            await asyncio.sleep(0)
            task.cancel()
            await asyncio.sleep(0)
            assert not task.done()
        finally:
            release.set()
            await asyncio.gather(task, return_exceptions=True)
            await fs.close_async()
        assert task.cancelled()
        assert events == ['completed', 'root://example//tmp/data',
                          'root://example//tmp']
        assert '/tmp' not in fs.dircache

    asyncio.run(run())


@pytest.mark.parametrize('start,end', [(-4, -1), (None, -1), (3, 1),
                                       (100, 120), (None, None), (-100, 100)])
def test_range_boundaries_match_python_slices(fs, start, end):
    path = '/tmp/fsspec-slice-' + uuid.uuid4().hex
    data = b'0123456789'
    fs.pipe_file(path, data)
    try:
        assert fs.cat_file(path, start, end) == data[start:end]
        assert fs.cat_ranges([path], [start], [end]) == [data[start:end]]
    finally:
        fs.rm_file(path)


def test_transfer_modes_callbacks_and_validation(fs, tmp_path):
    from fsspec.callbacks import Callback

    path = '/tmp/fsspec-transfer-' + uuid.uuid4().hex
    source, target = tmp_path / 'source', tmp_path / 'target'
    source.write_bytes(b'payload')
    upload, download = Callback(), Callback()
    try:
        fs.put_file(str(source), path, mode='create', callback=upload)
        assert upload.value == 7
        with pytest.raises(FileExistsError):
            fs.put_file(str(source), path, mode='create')
        fs.get_file(path, str(target), chunk_size=2, callback=download)
        assert download.value == 7
        assert target.read_bytes() == source.read_bytes()
        for operation in [lambda: fs.put_file(str(source), path, mode='bad'),
                          lambda: fs.pipe_file(path, b'no', mode='bad'),
                          lambda: fs.get_file(path, str(target), chunk_size=0),
                          lambda: fs.cp_file(path, path)]:
            with pytest.raises(ValueError):
                operation()
        assert fs.cat_file(path) == b'payload'
        assert target.read_bytes() == b'payload'
        with pytest.raises(ValueError):
            fs.open(path, 'invalid')
        with pytest.raises(ValueError, match='another XRootD host'):
            fs.info('root://other.example//data')
        with pytest.raises(TypeError):
            fs.cat_ranges(path, 0, 1)
        with pytest.raises(ValueError):
            fs.cat_ranges([path], [0, 1], [2])
        with pytest.raises(NotImplementedError):
            fs.cat_ranges([path], 0, 1, max_gap=1)
        result = fs.cat_ranges([path, path + '-missing'], 0, 3)
        assert result[0] == b'pay'
        assert isinstance(result[1], FileNotFoundError)
        with pytest.raises(FileNotFoundError):
            fs.cat_ranges([path + '-missing'], 0, 3, on_error='raise')
    finally:
        fs.rm_file(path)


@pytest.mark.parametrize('flags,kind,permissions', [
    (0, 'file', 0),
    (StatInfoFlags.IS_READABLE | StatInfoFlags.IS_WRITABLE, 'file', 0o644),
    (StatInfoFlags.IS_DIR | StatInfoFlags.IS_READABLE, 'directory', 0o555),
    (StatInfoFlags.OTHER, 'other', 0),
])
def test_metadata_for_older_servers(flags, kind, permissions):
    info = SimpleNamespace(flags=flags, size=42, modtime=10, id='opaque')
    result = _statinfo_to_info('/data', info)
    assert result['type'] == kind
    assert stat.S_IMODE(result['mode']) == permissions
    assert result['mtime'] == result['ctime'] == result['atime'] == 10
    assert result['uid'] == result['gid'] == 0
    assert result['ino'] == 'opaque'
    info.id, info.owner, info.group, info.mode = '12', '34', '56', '0640'
    result = _statinfo_to_info('/data', info)
    assert result['ino'] == 12
    assert result['uid'] == 34 and result['gid'] == 56
    assert stat.S_IMODE(result['mode']) == 0o640


@pytest.mark.parametrize('malformed', ['count', 'offset', 'size'])
def test_invalid_vector_responses_release_handles(malformed):
    async def run():
        closed = []

        class File:
            async def stat(self, force):
                return SimpleNamespace(size=4)

            async def vector_read(self, chunks, timeout):
                if malformed == 'count':
                    return []
                offset = 1 if malformed == 'offset' else 0
                return [SimpleNamespace(offset=offset,
                                        buffer=b'x' if malformed == 'size'
                                        else b'data')]

            async def close(self, timeout):
                closed.append(True)

        async def open_file(url, timeout):
            return File()

        async def limits(file):
            return 1, 4

        fs = XRootDFileSystem(hostid='example', asynchronous=True,
                              skip_instance_cache=True,
                              filehandle_cache_size=0, filehandle_cache_ttl=0)
        fs._read_handles.open_file = open_file
        fs._vector_limits = limits
        try:
            with pytest.raises(OSError, match='wrong chunk count|incomplete'):
                await fs._vector_read_ranges('/data', [(0, 4)])
            assert closed == [True]
        finally:
            await fs.close_async()

    asyncio.run(run())


def test_cache_open_failure_can_retry_and_expired_handle_is_replaced():
    async def run():
        opened, closed = [], []

        class File:
            async def close(self, timeout):
                closed.append(self)

        async def open_file(url, timeout):
            opened.append(url)
            if len(opened) == 1:
                raise PermissionError('denied')
            return File()

        cache = _ReadHandleCache(open_file, ttl=0)
        with pytest.raises(PermissionError):
            await cache.acquire('/data', 0)
        assert not cache.pending
        first = await cache.acquire('/data', 0)
        # Mark idle explicitly so expiry is deterministic without sleeping.
        first.users = 0
        first.accessed = float('-inf')
        second = await cache.acquire('/data', 0)
        assert second.file is not first.file
        assert closed == [first.file]
        await cache.release(second, 0)
        assert closed == [first.file, second.file]
        await cache.close(0)

    asyncio.run(run())


@pytest.mark.parametrize('all_paths', [False, True])
def test_invalidation_keeps_active_leases_until_release(all_paths):
    async def run():
        closed = []

        class File:
            async def close(self, timeout):
                closed.append(self)

        async def open_file(url, timeout):
            return File()

        cache = _ReadHandleCache(open_file, ttl=0)
        entry = await cache.acquire('/data', 0)
        if all_paths:
            await cache.invalidate_all(0)
        else:
            await cache.invalidate('/data', 0)
        assert not closed
        assert not cache.handles
        await cache.release(entry, 0)
        assert closed == [entry.file]
        await cache.close(0)

    asyncio.run(run())


def test_cache_attempts_every_close_after_an_error():
    async def run():
        closed = []

        class File:
            def __init__(self, url):
                self.url = url

            async def close(self, timeout):
                closed.append(self.url)
                if self.url == 'first':
                    raise OSError('close failed')

        async def open_file(url, timeout):
            return File(url)

        cache = _ReadHandleCache(open_file, ttl=3600)
        first = await cache.acquire('first', 0)
        second = await cache.acquire('second', 0)
        await cache.release(first, 0)
        await cache.release(second, 0)
        with pytest.raises(OSError, match='close failed'):
            await cache.close(0)
        assert closed == ['first', 'second']
        assert not cache.handles

    asyncio.run(run())


def test_cache_prune_error_does_not_leak_a_lease():
    async def run():
        class File:
            def __init__(self, url):
                self.url = url

            async def close(self, timeout):
                if self.url == 'victim':
                    raise OSError('close failed')

        async def open_file(url, timeout):
            return File(url)

        cache = _ReadHandleCache(open_file, max_items=None, ttl=3600)
        victim = await cache.acquire('victim', 0)
        keep = await cache.acquire('keep', 0)
        await cache.release(victim, 0)
        await cache.release(keep, 0)
        cache.max_items = 1
        try:
            with pytest.raises(OSError, match='close failed'):
                await cache.acquire('keep', 0)
            assert keep.users == 0
        finally:
            await cache.close(0)

    asyncio.run(run())


@pytest.mark.parametrize('stage', ['open-get', 'open-put', 'read', 'write'])
def test_cancelled_local_transfer_drains_thread_and_closes(monkeypatch, stage):
    import threading
    from XRootD.client import fsspec as module

    async def run():
        started = asyncio.Event()
        release = threading.Event()
        loop = asyncio.get_running_loop()
        events = []

        def pause():
            loop.call_soon_threadsafe(started.set)
            assert release.wait(3)
            events.append('completed')

        class Local:
            def read(self, size):
                if stage == 'read':
                    pause()
                return b''

            def write(self, data):
                if stage == 'write':
                    pause()

            def close(self):
                events.append('local-close')

        def open_local(*args):
            if stage.startswith('open'):
                pause()
            return Local()

        class Remote:
            async def __aenter__(self):
                return self

            async def __aexit__(self, *args):
                events.append('remote-close')

            async def read(self, size):
                return b'data'

            async def write(self, data):
                pass

        async def open_remote(*args):
            return Remote()

        monkeypatch.setattr(module, 'open', open_local, raising=False)
        fs = XRootDFileSystem(hostid='example', asynchronous=True,
                              skip_instance_cache=True)
        fs.open_async = open_remote
        upload = stage in ('open-put', 'read')
        transfer = fs._put_file('local', '/data') if upload else \
            fs._get_file('/data', 'local')
        task = asyncio.create_task(transfer)
        try:
            await asyncio.wait_for(started.wait(), 2)
            task.cancel()
            await asyncio.sleep(0)
            task.cancel()
            await asyncio.sleep(0)
            assert not task.done()
            assert 'local-close' not in events
        finally:
            release.set()
            await asyncio.gather(task, return_exceptions=True)
            await fs.close_async()
        assert task.cancelled()
        assert events.index('completed') < events.index('local-close')
        assert events.count('local-close') == 1

    asyncio.run(run())


def test_range_failure_waits_for_other_files_to_finish():
    async def run():
        started, release = asyncio.Event(), asyncio.Event()
        fs = XRootDFileSystem(hostid='example', asynchronous=True,
                              skip_instance_cache=True)

        async def read(path, ranges, batch_size):
            if path == '/bad':
                await started.wait()
                raise OSError('failed range')
            started.set()
            await release.wait()
            return [b'data']

        fs._vector_read_ranges = read
        task = asyncio.create_task(fs._cat_ranges(
            ['/bad', '/slow'], 0, 4, on_error='raise'))
        try:
            await asyncio.wait_for(started.wait(), 2)
            for _ in range(5):
                await asyncio.sleep(0)
            assert not task.done()
        finally:
            release.set()
        with pytest.raises(OSError, match='failed range'):
            await task
        await fs.close_async()

    asyncio.run(run())


@pytest.mark.parametrize('reply', [b'0 4', '4 0', b'garbage', b'\xff', '8 16'])
def test_vector_limits_validate_and_cache_server_reply(monkeypatch, reply):
    async def run():
        calls = []

        class Server:
            def __init__(self, endpoint):
                pass

            async def query(self, *args):
                calls.append(args)
                return reply

        fs = XRootDFileSystem(hostid='example', asynchronous=True,
                              skip_instance_cache=True)
        monkeypatch.setattr(aio, 'FileSystem', Server)
        file = SimpleNamespace(native=SimpleNamespace(
            get_property=lambda key: 'root://server:1094/'))
        expected = (8, 16) if reply == '8 16' else (1024, 2097136)
        assert await fs._vector_limits(file) == expected
        assert await fs._vector_limits(file) == expected
        assert len(calls) == 1
        file.native.get_property = lambda key: ''
        assert await fs._vector_limits(file) == (1024, 2097136)
        await fs.close_async()

    asyncio.run(run())


def test_async_cache_api_and_stream_alias(fs):
    path = '/tmp/fsspec-api-' + uuid.uuid4().hex
    fs.pipe_file(path, b'content')

    async def run():
        async_fs = XRootDFileSystem(hostid=fs.hostid, asynchronous=True,
                                    skip_instance_cache=True)
        try:
            with pytest.raises(RuntimeError, match='close_async'):
                async_fs.close()
            async with await async_fs.open_async(path) as stream:
                assert await stream.read(2) == b'co'
                assert stream.loc == 2
            await async_fs.invalidate_cache(path)
            url = fs.unstrip_protocol(path)
            assert async_fs._get_kwargs_from_urls(url) == {'hostid': fs.hostid}
        finally:
            await async_fs.close_async()

    try:
        asyncio.run(run())
    finally:
        fs.rm_file(path)


@pytest.mark.parametrize('success', [False, True])
@pytest.mark.parametrize('asynchronous', [False, True])
def test_alternate_sources_preserve_open_errors(monkeypatch, success,
                                                asynchronous):
    from XRootD.client.responses import XRootDStatus

    denied = XRootDStatus(dict(ok=False, code=XRootDStatus.errErrorResponse,
                               errno=3010, message='denied'))
    missing = XRootDStatus(dict(ok=False, code=XRootDStatus.errNotFound,
                                errno=0, message='missing'))
    ok = XRootDStatus(dict(ok=True, code=0, errno=0, message='ok'))
    attempts, closed = [], []

    async def sources(path):
        return ['root://first//data', 'root://second//data']

    def open_status(url):
        attempts.append(url)
        if url.startswith('root://second') and success:
            return ok
        return denied if url.startswith('root://example') else missing

    class SyncFile:
        def open(self, url, *args, **kwargs):
            return open_status(url), None

        def close(self, *args, **kwargs):
            closed.append(True)
            return ok, None

    class AsyncFile:
        async def open(self, url, *args, **kwargs):
            open_status(url).raise_on_error()
            return self

        async def close(self, timeout):
            closed.append(True)

    fs = XRootDFileSystem(hostid='example', asynchronous=asynchronous,
                          skip_instance_cache=True)
    fs._source_urls = sources
    if asynchronous:
        monkeypatch.setattr(aio, 'File', AsyncFile)

        async def run():
            try:
                if success:
                    async with await fs.open_async('/data'):
                        pass
                else:
                    with pytest.raises(PermissionError):
                        await fs.open_async('/data')
            finally:
                await fs.close_async()

        asyncio.run(run())
    else:
        monkeypatch.setattr(client, 'File', SyncFile)
        try:
            if success:
                with fs.open('/data', 'rb', size=4) as file:
                    assert file.size == 4
                file.close()
            else:
                with pytest.raises(PermissionError):
                    fs.open('/data', 'rb', size=4)
        finally:
            fs.close()
    assert len(attempts) == 3
    if success:
        assert closed == [True]


def test_source_discovery_filters_and_deduplicates():
    async def run():
        fs = XRootDFileSystem(hostid='example', asynchronous=True,
                              valid_sources=['allowed'],
                              skip_instance_cache=True)

        async def locate(*args):
            return [SimpleNamespace(address=name) for name in
                    ['blocked:1094', 'allowed:1094', 'allowed:1094']]

        fs._client.locate = locate
        assert await fs._source_urls('/data') == ['root://allowed:1094//data']
        fs.valid_sources = {'absent'}
        with pytest.raises(OSError, match='no allowed'):
            await fs._source_urls('/data')
        await fs.close_async()

    asyncio.run(run())


def test_close_reports_background_pruning_errors():
    async def run():
        closed = []
        failure = OSError('background close failed')

        class File:
            def __init__(self, url):
                self.url = url

            async def close(self, timeout):
                closed.append(self.url)
                if self.url == '/failed':
                    raise failure

        async def open_file(url, timeout):
            return File(url)

        async def wait_until(predicate):
            while not predicate():
                await asyncio.sleep(0)

        cache = _ReadHandleCache(open_file, ttl=0.01)
        entry = await cache.acquire('/failed', 0)
        await cache.release(entry, 0)
        await asyncio.wait_for(
            wait_until(lambda: cache._pruner_error is not None), 2)
        assert not cache._pruner_task.done()
        # A different server remains readable, and idle pruning continues.
        entry = await cache.acquire('/healthy', 0)
        await cache.release(entry, 0)
        await asyncio.wait_for(
            wait_until(lambda: '/healthy' in closed), 2)
        active = await cache.acquire('/active', 0)
        with pytest.raises(OSError) as caught:
            await cache.close(0)
        assert caught.value is failure
        assert not cache.handles
        assert '/active' not in closed
        await cache.release(active, 0)
        assert '/active' in closed
        # The deferred failure is reported once; the cache is reusable.
        await cache.close(0)
        entry = await cache.acquire('/reopened', 0)
        await cache.release(entry, 0)
        await cache.close(0)
        assert '/reopened' in closed

    asyncio.run(run())


@pytest.mark.parametrize('path', [None, '/data'])
@pytest.mark.parametrize('cancel_close', [False, True])
def test_scheduled_invalidation_is_owned_and_drained(
        monkeypatch, path, cancel_close):
    async def run():
        fs = XRootDFileSystem(hostid='localhost', asynchronous=True,
                              skip_instance_cache=True)
        started = asyncio.Event()
        events = []
        pending = None

        async def invalidate(*args):
            nonlocal pending
            future = asyncio.get_running_loop().create_future()
            pending = weakref.ref(future)
            started.set()
            await future
            events.append('invalidated')

        async def close(timeout):
            events.append('closed')

        monkeypatch.setattr(fs._read_handles, 'invalidate', invalidate)
        monkeypatch.setattr(fs._read_handles, 'invalidate_all', invalidate)
        monkeypatch.setattr(fs._read_handles, 'close', close)
        task = fs.invalidate_cache(path)
        retained = weakref.ref(task)
        del task  # fsspec buffered-file close discards this return value.
        await asyncio.wait_for(started.wait(), 2)
        gc.collect()
        assert retained() is not None
        assert not retained().done()
        close_task = asyncio.create_task(fs.close_async())
        try:
            await asyncio.sleep(0)
            if cancel_close:
                close_task.cancel()
                await asyncio.sleep(0)
                close_task.cancel()
            assert events == []
            pending().set_result(None)
            if cancel_close:
                with pytest.raises(asyncio.CancelledError):
                    await asyncio.wait_for(close_task, 2)
            else:
                await asyncio.wait_for(close_task, 2)
            assert events == ['invalidated', 'closed']
            assert not fs._invalidation_tasks
        finally:
            if pending() is not None and not pending().done():
                pending().set_result(None)
            await asyncio.gather(close_task, return_exceptions=True)
            await fs.close_async()

    asyncio.run(run())


def test_close_reports_scheduled_invalidation_errors(monkeypatch):
    async def run():
        fs = XRootDFileSystem(hostid='localhost', asynchronous=True,
                              skip_instance_cache=True)
        failure = OSError('scheduled invalidation failed')
        events = []
        unhandled = []
        loop = asyncio.get_running_loop()
        loop.set_exception_handler(lambda loop, context: unhandled.append(
            context))

        async def invalidate(url, timeout):
            if url.endswith('/failed'):
                raise failure
            if url.endswith('/also-failed'):
                raise OSError('second invalidation failed')
            events.append('invalidated')

        async def close(timeout):
            events.append('closed')

        monkeypatch.setattr(fs._read_handles, 'invalidate', invalidate)
        monkeypatch.setattr(fs._read_handles, 'close', close)
        fs.invalidate_cache('/failed')
        fs.invalidate_cache('/also-failed')
        fs.invalidate_cache('/healthy')
        with pytest.raises(OSError) as caught:
            await fs.close_async()
        assert caught.value is failure
        assert events == ['invalidated', 'closed']
        assert not fs._invalidation_tasks
        gc.collect()
        assert not unhandled
        # The deferred error is reported once, and cleanup remains reusable.
        await fs.close_async()
        assert events == ['invalidated', 'closed', 'closed']

    asyncio.run(run())


def test_cancelled_scheduled_invalidation_is_released(monkeypatch):
    async def run():
        fs = XRootDFileSystem(hostid='localhost', asynchronous=True,
                              skip_instance_cache=True)
        started, release = asyncio.Event(), asyncio.Event()

        async def invalidate(url, timeout):
            started.set()
            await release.wait()

        monkeypatch.setattr(fs._read_handles, 'invalidate', invalidate)
        task = fs.invalidate_cache('/data')
        await asyncio.wait_for(started.wait(), 2)
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task
        await fs.close_async()
        assert not fs._invalidation_tasks

    asyncio.run(run())


def test_filesystem_constructed_without_current_event_loop():
    from concurrent.futures import ThreadPoolExecutor

    def use_filesystem():
        # A fresh thread has no implicit event loop, including on Python 3.9.
        fs = XRootDFileSystem(hostid=client.URL(SERVER_URL).hostid,
                              skip_instance_cache=True)
        path = '/tmp/no-event-loop-' + uuid.uuid4().hex
        try:
            fs.pipe_file(path, b'content')
            assert fs.cat_file(path) == b'content'
            fs.rm(path)
        finally:
            fs.close()

    with ThreadPoolExecutor(max_workers=1) as executor:
        executor.submit(use_filesystem).result(timeout=10)


def test_cancelled_invalidation_finishes_all_idle_closes():
    async def run():
        started, release = asyncio.Event(), asyncio.Event()
        closed = []

        class File:
            def __init__(self, url):
                self.url = url

            async def close(self, timeout):
                if self.url == 'first':
                    started.set()
                    await release.wait()
                closed.append(self.url)

        async def open_file(url, timeout):
            return File(url)

        cache = _ReadHandleCache(open_file, ttl=3600)
        first = await cache.acquire('first', 0)
        second = await cache.acquire('second', 0)
        await cache.release(first, 0)
        await cache.release(second, 0)
        task = asyncio.create_task(cache.invalidate_all(0))
        try:
            await asyncio.wait_for(started.wait(), 2)
            task.cancel()
            await asyncio.sleep(0)
            release.set()
            with pytest.raises(asyncio.CancelledError):
                await asyncio.wait_for(task, 2)
            assert closed == ['first', 'second']
        finally:
            release.set()
            await cache.close(0)

    asyncio.run(run())
