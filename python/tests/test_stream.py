"""Standard Python file behavior over a local XRootD server."""

import builtins
import io
import uuid

import pytest

from XRootD import client
from env import SERVER_URL


def remote_path():
    return SERVER_URL + '/tmp/stream-' + uuid.uuid4().hex


def remove(path):
    client.FileSystem(SERVER_URL).rm(client.URL(path).path)


def test_star_import_preserves_builtin_open():
    namespace = {'open': builtins.open}
    exec('from XRootD.client import *', namespace)
    assert namespace['open'] is builtins.open
    for name in ('File', 'FileSystem', 'CopyProcess', 'URL', 'glob',
                 'EnvGetString', 'raise_on_error', 'XRootDError'):
        assert namespace[name] is getattr(client, name)
    assert callable(client.open)


def test_binary_file_object():
    path = remote_path()
    try:
        with client.open(path, 'wb') as file:
            assert isinstance(file, io.BufferedWriter)
            assert file.write(b'abcdef') == 6
            assert not file.closed
        assert file.closed

        with client.open(path, 'rb') as file:
            assert file.read(2) == b'ab'
            assert file.tell() == 2
            assert file.seek(-2, io.SEEK_END) == 4
            assert file.read() == b'ef'

        with client.open(path, 'r+b', buffering=0) as file:
            assert file.seek(2) == 2
            assert file.write(b'XY') == 2
            assert file.seek(0) == 0
            output = bytearray(6)
            assert file.readinto(output) == 6
            assert output == b'abXYef'

        with client.open(path, 'ab') as file:
            file.seek(0)
            file.write(b'!')
        with client.open(path, 'rb') as file:
            assert file.read() == b'abXYef!'
    finally:
        remove(path)


def test_text_file_object():
    path = remote_path()
    try:
        with client.open(path, 'w', encoding='utf-8') as file:
            assert isinstance(file, io.TextIOWrapper)
            file.write('éclair\nsecond\n')
        with client.open(path, 'r', encoding='utf-8') as file:
            assert list(file) == ['éclair\n', 'second\n']
    finally:
        remove(path)


def test_standard_errors_and_mode_validation():
    missing = remote_path()
    with pytest.raises(FileNotFoundError) as error:
        client.open(missing, 'rb')
    assert error.value.filename == missing
    assert error.value.xrootd_status is not None

    with pytest.raises(ValueError):
        client.open(missing, 'bb')
    with pytest.raises(ValueError):
        client.open(missing, 'r', buffering=0)
    with pytest.raises(ValueError):
        client.open(missing, 'rb', encoding='utf-8')


def test_exclusive_create_and_append_new_file():
    exclusive = remote_path()
    append = remote_path()
    try:
        with client.open(exclusive, 'xb') as file:
            file.write(b'first')
        with pytest.raises(FileExistsError):
            client.open(exclusive, 'xb')
        with client.open(append, 'ab') as file:
            file.write(b'created')
        with client.open(append, 'rb') as file:
            assert file.read() == b'created'
    finally:
        remove(exclusive)
        remove(append)


def test_seek_end_after_write_keeps_existing_bytes():
    path = remote_path()
    try:
        with client.open(path, 'w+b', buffering=0) as file:
            file.write(b'root')
            assert file.seek(0, io.SEEK_END) == 4
            file.write(b'\0' * 16)
            file.seek(0)
            assert file.read(4) == b'root'
        with client.open(path, 'rb') as file:
            assert file.read() == b'root' + b'\0' * 16
    finally:
        remove(path)


def test_invalid_arguments_do_not_modify_file_or_cursor():
    path = remote_path()

    class Index:
        def __index__(self):
            return 2

    try:
        with client.open(path, 'wb') as file:
            file.write(b'abcdef')
        with pytest.raises(TypeError):
            client.open(path, 'wb', buffering=2.5)
        with client.open(path, 'r+b', buffering=0) as file:
            assert file.read() == b'abcdef'
            assert file.seek(Index()) == 2
            with pytest.raises(TypeError):
                file.seek(1.5)
            assert file.tell() == 2
            with pytest.raises(TypeError):
                file.readinto(b'readonly')
            assert file.tell() == 2
            with pytest.raises(TypeError):
                file.truncate(2.5)
            with pytest.raises(ValueError):
                file.truncate(-1)
            assert file.read() == b'cdef'
            assert file.truncate(Index()) == 2
        methods = file.readable, file.writable, file.seekable, file.flush
        for method in methods:
            with pytest.raises(ValueError):
                method()
        with client.open(path) as file:
            assert file.read() == b'ab'
    finally:
        remove(path)


@pytest.mark.parametrize('mode', [None, '', 'rw', 'rbt', 'zz'])
def test_invalid_modes_never_open(mode):
    expected = TypeError if mode is None else ValueError
    with pytest.raises(expected):
        client.open(remote_path(), mode)


def test_raw_stream_permissions_zero_reads_and_truncate():
    path = remote_path()
    try:
        with client.open(path, 'wb', buffering=0) as file:
            with pytest.raises(io.UnsupportedOperation):
                file.read(1)
            file.write(b'abcdef')
            file.seek(3)
            assert file.truncate() == 3
            with pytest.raises(ValueError):
                file.seek(0, 99)
        with client.open(path, 'rb', buffering=0) as file:
            assert file.read(0) == b''
            with pytest.raises(io.UnsupportedOperation):
                file.write(b'no')
            with pytest.raises(io.UnsupportedOperation):
                file.truncate(1)
            assert file.read() == b'abc'
        with client.open(path, 'r', buffering=1) as file:
            assert file.read() == 'abc'
    finally:
        remove(path)


def test_wrapper_failure_closes_raw_file(monkeypatch):
    from XRootD.client import stream

    path = remote_path()
    opened = []
    original = stream.File

    def track():
        file = original()
        opened.append(file)
        return file

    monkeypatch.setattr(stream, 'File', track)
    try:
        with client.open(path, 'wb') as file:
            file.write(b'data')
        with pytest.raises(LookupError):
            client.open(path, 'r', encoding='no-such-encoding')
        assert all(not file.is_open() for file in opened)
        with pytest.raises(ValueError):
            client.open(path, 'wb', buffering=-2)
        assert len(opened) == 2
    finally:
        remove(path)
