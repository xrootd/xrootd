"""Awaitable filesystem helpers preserve metadata and Python errors."""

import asyncio
import uuid
from types import SimpleNamespace

import pytest

from XRootD.client import aio
from XRootD.client.responses import XRootDAuthorizationError, XRootDStatus
from env import SERVER_URL


def run_async(coroutine):
    loop = asyncio.new_event_loop()
    try:
        return loop.run_until_complete(coroutine)
    finally:
        loop.close()


def test_async_filesystem_helpers():
    async def run():
        fs = aio.FileSystem(SERVER_URL)
        root = '/tmp/aio-helpers-' + uuid.uuid4().hex
        directory = root + '/nested'
        path = directory + '/file'
        assert not await fs.exists(root)
        assert not await fs.is_file(root)
        assert not await fs.is_dir(root)
        await fs.makedirs(directory)
        try:
            assert await fs.is_dir(directory)
            await fs.makedirs(directory, exist_ok=True)
            with pytest.raises(FileExistsError):
                await fs.makedirs(directory)
            async with aio.open(SERVER_URL + path, 'wb') as file:
                await file.write(b'hello')
            assert await fs.exists(path)
            assert await fs.is_file(path)
            assert not await fs.is_dir(path)
            assert await fs.listdir(directory) == ['file']
            entries = await fs.scandir(directory)
            assert len(entries) == 1
            assert entries[0].path == path
            assert entries[0].is_file()
            assert entries[0].size == 5
            with pytest.raises(FileExistsError):
                await fs.makedirs(path, exist_ok=True)
            await fs.unlink(path)
            await fs.unlink(path, missing_ok=True)
            with pytest.raises(FileNotFoundError) as error:
                await fs.unlink(path)
            assert error.value.filename == path
            assert error.value.xrootd_status is not None
        finally:
            await fs.unlink(path, missing_ok=True)
            await fs.rmdir(directory)
            await fs.rmdir(root)

    run_async(run())


def test_async_helpers_preserve_permission_errors(monkeypatch):
    async def run():
        fs = aio.FileSystem(SERVER_URL)
        status = XRootDStatus({'ok': False, 'code': XRootDStatus.errAuthFailed,
                              'errno': 0, 'message': 'denied'})

        async def denied(*args, **kwargs):
            raise XRootDAuthorizationError(status)

        monkeypatch.setattr(fs, 'stat', denied)
        monkeypatch.setattr(fs, 'rm', denied)
        for check in (fs.exists, fs.is_file, fs.is_dir):
            with pytest.raises(PermissionError):
                await check('/secret')
        with pytest.raises(PermissionError):
            await fs.unlink('/secret', missing_ok=True)
        with pytest.raises(PermissionError):
            await fs.makedirs('/secret', exist_ok=True)

    run_async(run())


def test_async_scandir_fills_missing_metadata(monkeypatch):
    async def run():
        fs = aio.FileSystem(SERVER_URL)
        paths = []

        async def listing(*args):
            return [SimpleNamespace(name='file', statinfo=None)]

        async def stat(path, timeout):
            paths.append(path)
            return SimpleNamespace(flags=0, size=12)

        monkeypatch.setattr(fs, 'dirlist', listing)
        monkeypatch.setattr(fs, 'stat', stat)
        entries = await fs.scandir('//data?token=abc')
        assert paths == ['//data/file?token=abc']
        assert entries[0].size == 12

    run_async(run())


def test_async_checksum_selects_and_checks_algorithm(monkeypatch):
    async def run():
        fs = aio.FileSystem(SERVER_URL)
        queries = []

        async def query(code, path, timeout):
            queries.append(path)
            return b'crc32c 1234'

        monkeypatch.setattr(fs, 'query', query)
        assert await fs.checksum('/file?token=abc', 'crc32c') == \
            ('crc32c', '1234')
        assert queries == ['/file?token=abc&cks.type=crc32c']
        with pytest.raises(OSError, match='Expected md5'):
            await fs.checksum('/file', 'md5')

    run_async(run())


@pytest.mark.parametrize('is_directory', [True, False])
def test_async_makedirs_concurrent_creation(monkeypatch, is_directory):
    async def run():
        fs = aio.FileSystem(SERVER_URL)
        original = FileExistsError('concurrently created')

        async def missing(*args):
            return None

        async def created(*args):
            raise original

        async def is_dir(*args):
            await asyncio.sleep(0)
            return is_directory

        monkeypatch.setattr(fs, '_stat_if_exists', missing)
        monkeypatch.setattr(fs, 'mkdir', created)
        monkeypatch.setattr(fs, 'is_dir', is_dir)
        if is_directory:
            await fs.makedirs('/data', exist_ok=True)
        else:
            with pytest.raises(FileExistsError) as error:
                await fs.makedirs('/data', exist_ok=True)
            assert error.value is original

    run_async(run())
