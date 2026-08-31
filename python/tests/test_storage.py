from pathlib import Path

from XRootD.client import AuthContext, StorageClient
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

  assert result == {'size': 17}
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
