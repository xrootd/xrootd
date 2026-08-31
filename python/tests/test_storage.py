import os
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from threading import Event
from urllib.parse import parse_qs, urlsplit

import pytest

from XRootD.client import AuthContext, StorageClient, XRootDTimeoutError
import XRootD.client.storage as storage_module


class OkStatus(object):
  def raise_on_error(self):
    return self


def test_stat_preserves_signed_query_as_filesystem_property(monkeypatch):
  calls = []

  class NativeFileSystem(object):
    def __init__(self, endpoint, auth=None):
      calls.append(('endpoint', endpoint, auth))

    def set_property(self, name, value):
      calls.append(('property', name, value))

    def stat(self, path, timeout=0):
      calls.append(('stat', path, timeout))
      return OkStatus(), {'size': 17}

  monkeypatch.setattr(storage_module, 'FileSystem', NativeFileSystem)
  auth = AuthContext.anonymous()
  client = StorageClient(auth=auth, timeout=10)
  result = client.stat(
    'davs://storage.example/data/file?X-Amz-Signature=abc+def==&empty=')

  assert result.size == 17
  assert result.stat == {'size': 17}
  assert calls[0] == ('endpoint', 'davs://storage.example/', auth)
  assert calls[1] == (
    'property', 'XrdClHttpQueryParam', 'X-Amz-Signature=abc+def==&empty=')
  assert calls[2][0:2] == ('stat', '/data/file')
  assert 1 <= calls[2][2] <= 10


def test_put_uses_one_scoped_copy_job(monkeypatch, tmp_path):
  calls = []

  class NativeCopyProcess(object):
    def add_job(self, source, target, **options):
      calls.append(('job', source, target, options))

    def prepare(self):
      calls.append(('prepare',))
      return OkStatus()

    def run(self):
      calls.append(('run',))
      return OkStatus(), [{'status': OkStatus(), 'size': 4}]

  class NativeFileSystem(object):
    def __init__(self, endpoint, auth=None):
      calls.append(('filesystem', endpoint, auth))

    def mkdir_p(self, path, timeout=0):
      calls.append(('mkdir_p', path, timeout))
      return OkStatus(), None

  monkeypatch.setattr(storage_module, 'CopyProcess', NativeCopyProcess)
  monkeypatch.setattr(storage_module, 'FileSystem', NativeFileSystem)
  source = tmp_path / 'source'
  source.write_bytes(b'data')
  auth = AuthContext.x509(proxy='/tmp/proxy')
  result = StorageClient(auth=auth, timeout=20).put(
    source, 'davs://storage.example/data/file', create_parents=True)

  assert result['size'] == 4
  job = next(call for call in calls if call[0] == 'job')
  mkdir = next(call for call in calls if call[0] == 'mkdir_p')
  assert mkdir[1] == '/data'
  assert 1 <= mkdir[2] <= 20
  assert job[1] == Path(source).resolve().as_uri()
  assert job[2] == 'davs://storage.example/data/file'
  assert job[3]['mkdir'] is False
  assert job[3]['source_auth'] is auth
  assert job[3]['target_auth'] is auth


def test_space_returns_application_friendly_counts(monkeypatch):
  class NativeFileSystem(object):
    def __init__(self, endpoint, auth=None):
      pass

    def query(self, code, path, timeout=0):
      return OkStatus(), 'oss.space=100&oss.free=75&oss.used=25&oss.maxf=75'

  monkeypatch.setattr(storage_module, 'FileSystem', NativeFileSystem)
  assert StorageClient(timeout=10).space('https://storage.example/data') == {
    'total': 100, 'free': 75, 'used': 25,
  }


def test_checksum_returns_algorithm_and_value(monkeypatch):
  calls = []

  class NativeFileSystem(object):
    def __init__(self, endpoint, auth=None):
      calls.append(('endpoint', endpoint, auth))

    def checksum(self, path, timeout=0):
      calls.append(('checksum', path, timeout))
      return OkStatus(), type('Checksum', (), {
        'algorithm': 'adler32', 'value': 'deadbeef'})()

  monkeypatch.setattr(storage_module, 'FileSystem', NativeFileSystem)

  result = StorageClient(timeout=10).checksum(
    'root://storage.example/data/file', algorithm='adler32')

  assert result == ('adler32', 'deadbeef')
  assert calls[1][0:2] == (
    'checksum', '/data/file?cks.type=adler32')
  assert 1 <= calls[1][2] <= 10


def test_probe_performs_bounded_stat(monkeypatch):
  calls = []

  class NativeFileSystem(object):
    def __init__(self, endpoint, auth=None):
      pass

    def stat(self, path, timeout=0):
      calls.append((path, timeout))
      return OkStatus(), {'size': 0}

  monkeypatch.setattr(storage_module, 'FileSystem', NativeFileSystem)

  result = StorageClient(timeout=30).probe(
    'root://storage.example//rucio', timeout=7)

  assert result.size == 0
  assert calls[0][0] == '//rucio'
  assert 1 <= calls[0][1] <= 7


def test_info_negotiates_and_caches_checksum_algorithm(monkeypatch):
  checksum_paths = []

  class NativeFileSystem(object):
    def __init__(self, endpoint, auth=None):
      pass

    def stat(self, path, timeout=0):
      return OkStatus(), {'size': 23}

    def checksum(self, path, timeout=0):
      checksum_paths.append(path)
      algorithm = 'crc32c' if 'adler32' in path else 'md5'
      return OkStatus(), type('Checksum', (), {
        'algorithm': algorithm, 'value': 'deadbeef'})()

  monkeypatch.setattr(storage_module, 'FileSystem', NativeFileSystem)
  client = StorageClient(timeout=30)
  url = 'root://storage.example//rucio/file'

  first = client.info(
    url, checksum_algorithms=('adler32', 'md5'), require_checksum=True)
  second = client.info(
    url, checksum_algorithms=('adler32', 'md5'), require_checksum=True)

  assert first.size == 23
  assert first.checksum.algorithm == 'md5'
  assert first.checksum.value == 'deadbeef'
  assert second.checksum.algorithm == 'md5'
  assert checksum_paths == [
    '//rucio/file?cks.type=adler32',
    '//rucio/file?cks.type=md5',
    '//rucio/file?cks.type=md5',
  ]


def test_owned_environment_auth_is_closed_with_client():
  auth = AuthContext.bearer(token='secret')
  client = StorageClient(auth=auth, owns_auth=True, timeout=10)
  token_file = parse_qs(urlsplit(
    auth.apply('root://localhost//data')).query)['xrd.ztn'][0]
  assert os.path.exists(token_file)

  client.close()

  assert not os.path.exists(token_file)
  with pytest.raises(RuntimeError, match='closed'):
    client.exists('root://localhost//data')


def test_local_deadline_uses_xrootd_timeout_error():
  with pytest.raises(XRootDTimeoutError, match='deadline expired'):
    StorageClient._remaining(0)


def test_close_waits_for_active_operation(monkeypatch):
  started = Event()
  release = Event()

  class NativeFileSystem(object):
    def __init__(self, endpoint, auth=None):
      pass

    def stat(self, path, timeout=0):
      started.set()
      assert release.wait(timeout=5)
      return OkStatus(), {'size': 1}

  monkeypatch.setattr(storage_module, 'FileSystem', NativeFileSystem)
  client = StorageClient(timeout=10)
  with ThreadPoolExecutor(max_workers=2) as executor:
    operation = executor.submit(
      client.stat, 'root://storage.example//rucio/file')
    assert started.wait(timeout=5)
    closing = executor.submit(client.close)
    assert not closing.done()
    release.set()
    assert operation.result(timeout=5).size == 1
    closing.result(timeout=5)
