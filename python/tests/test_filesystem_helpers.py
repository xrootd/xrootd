"""Python-style filesystem helpers over a local XRootD server."""

import asyncio
import sys
import uuid
from types import SimpleNamespace

import pytest

from XRootD import client
from XRootD.client.responses import checksum_query_path, parse_checksum
from env import SERVER_URL


def test_directory_entry_preserves_remote_path_and_params():
    listed = SimpleNamespace(name='item', hostaddr='server')
    info = SimpleNamespace(flags=0, size=4)
    entry = client.DirectoryEntry('//data?svcClass=hot', listed, info)
    assert entry.path == '//data/item?svcClass=hot'
    assert entry.hostaddr == 'server'
    assert entry.is_file()
    assert entry.size == 4

    fs = client.FileSystem(SERVER_URL)
    with pytest.raises(ValueError, match='remote root'):
        fs.remove_tree('/')
    with pytest.raises(ValueError, match='remote root'):
        fs.remove_tree(SERVER_URL)


def test_filesystem_path_helpers():
    fs = client.FileSystem(SERVER_URL)
    root = '/tmp/helpers-' + uuid.uuid4().hex
    nested = root + '/child'
    path = nested + '/data'
    missing = nested + '/missing'

    try:
        assert not fs.exists(root)
        assert not fs.is_file(root)
        assert not fs.is_dir(root)
        fs.makedirs(nested)
        assert fs.exists(root)
        assert fs.is_dir(root)
        assert fs.is_dir(nested)
        assert not fs.is_file(nested)
        fs.makedirs(nested, exist_ok=True)
        with pytest.raises(FileExistsError):
            fs.makedirs(nested)

        with client.open(SERVER_URL + path, 'wb') as remote:
            remote.write(b'content')
        assert fs.exists(path)
        assert fs.is_file(path)
        assert not fs.is_dir(path)
        assert fs.listdir(nested) == ['data']
        assert not fs.exists(missing)
        with pytest.raises(FileNotFoundError) as error:
            fs.listdir(missing)
        assert error.value.xrootd_status.ok is False
        with pytest.raises(FileExistsError):
            fs.makedirs(path, exist_ok=True)
    finally:
        fs.rm(path)
        fs.rmdir(nested)
        fs.rmdir(root)


def test_scandir_and_remove_tree():
    fs = client.FileSystem(SERVER_URL)
    root = '/tmp/tree-' + uuid.uuid4().hex
    nested = root + '/child'
    first = root + '/first'
    second = nested + '/second'
    fs.makedirs(nested)
    try:
        with client.open(SERVER_URL + first, 'wb') as remote:
            remote.write(b'one')
        with client.open(SERVER_URL + second, 'wb') as remote:
            remote.write(b'two!')
        entries = {entry.name: entry for entry in fs.scandir(root)}
        assert entries['child'].path == nested
        assert entries['child'].is_dir()
        assert entries['first'].path == first
        assert entries['first'].is_file()
        assert entries['first'].size == 3
        assert entries['first'].stat().size == 3

        result = fs.remove_tree(root)
        assert result.as_dict() == {'FilesRemoved': 2,
                                    'DirectoriesRemoved': 2,
                                    'SizeRemoved': 7}
        assert not fs.exists(root)
        assert fs.remove_tree(root, missing_ok=True).files_removed == 0
        with pytest.raises(FileNotFoundError):
            fs.remove_tree(root)
    finally:
        if fs.exists(root):
            fs.remove_tree(root)


@pytest.mark.parametrize('params', ['', '?opaque=value'])
def test_remove_tree_accepts_urls(monkeypatch, params):
    fs = client.FileSystem(SERVER_URL)
    root = '/tmp/tree-url-' + uuid.uuid4().hex
    nested = root + '/child'
    with client.open(SERVER_URL + nested + '/file', 'wb') as remote:
        remote.write(b'payload')
    paths = []

    def capture(method):
        def call(path, *args, **kwargs):
            paths.append(path)
            return method(path, *args, **kwargs)
        return call

    try:
        for name in ('stat', 'dirlist', 'rm', 'rmdir'):
            monkeypatch.setattr(fs, name, capture(getattr(fs, name)))
        result = fs.remove_tree(SERVER_URL + root + params)
        monkeypatch.undo()
        assert result.as_dict() == {'FilesRemoved': 1,
                                    'DirectoriesRemoved': 2,
                                    'SizeRemoved': 7}
        assert paths
        assert all(not path.startswith('root://') for path in paths)
        if params:
            assert all(path.endswith(params) for path in paths)
        assert not fs.exists(root)
    finally:
        monkeypatch.undo()
        fs.remove_tree(root, missing_ok=True)


@pytest.mark.parametrize('algorithm', ['md5\n', 'md5\r', 'md5\r\n',
                                       'md5\x00', 'md5&token=bad'])
def test_checksum_rejects_control_characters(algorithm):
    with pytest.raises(ValueError, match='checksum algorithm'):
        checksum_query_path('/file?token=abc', algorithm)


def test_checksum_parser_and_filesystem_helper(monkeypatch):
    assert checksum_query_path('/file?token=abc&cks.type=md5', 'crc32c') == \
        '/file?token=abc&cks.type=crc32c'
    assert checksum_query_path('/file?token=abc', None) == \
        '/file?token=abc'
    with pytest.raises(ValueError, match='checksum algorithm'):
        checksum_query_path('/file', 'md5&token=bad')
    assert parse_checksum(b'adler32 deadbeef\x00') == ('adler32', 'deadbeef')
    assert parse_checksum('ADLER32 deadbeef', 'adler32') == \
        ('ADLER32', 'deadbeef')
    with pytest.raises(OSError, match='Expected md5'):
        parse_checksum('adler32 deadbeef', 'md5')
    with pytest.raises(OSError, match='Invalid'):
        parse_checksum('bad')

    fs = client.FileSystem(SERVER_URL)
    status, _ = fs.stat('/tmp')
    assert status.ok
    queries = []

    def query(code, path, **kwargs):
        queries.append(path)
        return status, b'adler32 deadbeef'

    monkeypatch.setattr(fs, 'query', query)
    assert fs.checksum('/tmp/file?token=abc&cks.type=md5', 'adler32') == \
        ('adler32', 'deadbeef')
    assert queries == ['/tmp/file?token=abc&cks.type=adler32']


def test_copy_one_handles_job_status():
    fs = client.FileSystem(SERVER_URL)
    source = '/tmp/copy-one-' + uuid.uuid4().hex
    target = source + '-target'
    try:
        with client.open(SERVER_URL + source, 'wb') as remote:
            remote.write(b'copied')
        result = client.CopyProcess.copy_one(
            SERVER_URL + source, SERVER_URL + target,
            force=True, mkdir=True)
        assert isinstance(result, dict)
        with client.open(SERVER_URL + target, 'rb') as remote:
            assert remote.read() == b'copied'
        with pytest.raises(OSError):
            client.CopyProcess.copy_one(
                SERVER_URL + source + '-missing', SERVER_URL + target,
                force=True)
    finally:
        for path in (source, target):
            if fs.exists(path):
                fs.rm(path)


@pytest.mark.parametrize('asynchronous', [False, True])
def test_stat_info_and_unlink_contract(asynchronous):
    fs_type = client.FileSystem
    if asynchronous:
        if sys.version_info < (3, 11):
            pytest.skip('native asyncio interfaces require Python >= 3.11')
        from XRootD.client import aio
        fs_type = aio.FileSystem

    async def run():
        fs = fs_type(SERVER_URL)

        async def call(name, *args, **kwargs):
            result = getattr(fs, name)(*args, **kwargs)
            return await result if asynchronous else result

        path = '/tmp/stat-unlink-' + uuid.uuid4().hex
        directory = path + '-directory'
        with client.open(SERVER_URL + path, 'wb') as file:
            file.write(b'data')
        with client.open(SERVER_URL + directory + '/child', 'wb') as file:
            file.write(b'keep directory nonempty')
        try:
            assert (await call('stat_info', path, timeout=5)).size == 4
            await call('unlink', path, timeout=5)
            await call('unlink', path, missing_ok=True, timeout=5)
            for method in ('stat_info', 'unlink'):
                with pytest.raises(FileNotFoundError) as caught:
                    await call(method, path, timeout=5)
                assert caught.value.filename == path
                assert not caught.value.xrootd_status.ok
            # Use our own nonempty directory: /tmp may be empty when CTest
            # runs this module alone; the local server can remove empty dirs.
            with pytest.raises(OSError):
                await call('unlink', directory, missing_ok=True, timeout=5)
        finally:
            await call('unlink', path, missing_ok=True)
            client.FileSystem(SERVER_URL).remove_tree(
                directory, missing_ok=True)

    loop = asyncio.new_event_loop()
    try:
        loop.run_until_complete(run())
    finally:
        loop.close()
