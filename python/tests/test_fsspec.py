from __future__ import absolute_import, division, print_function

import asyncio
import errno
import inspect

import pytest

pytest.importorskip('fsspec')
import fsspec

try:
  FileNotFoundError
except NameError:
  FileNotFoundError = IOError

try:
  FileExistsError
except NameError:
  FileExistsError = OSError

from XRootD.client.flags import DirListFlags, MkDirFlags, OpenFlags, QueryCode
from XRootD.client.flags import StatInfoFlags
from XRootD.client.responses import XRootDStatus
import XRootD.fsspec as fsspec_module
from XRootD.fsspec import DEFAULT_ACCESS_MODE, SUPPORTED_PROTOCOLS
from XRootD.fsspec import XRootDFileSystem
from XRootD.fsspec import _info_from_stat, _raise_if_error


class FakeStatus(object):
  def __init__(self, ok=True, code=0, errno_value=0, message=''):
    self.ok = ok
    self.code = code
    self.errno = errno_value
    self.message = message

  def __str__(self):
    return self.message


class FakeStatInfo(object):
  def __init__(self, flags, size=0, modtime=0, owner=None, group=None):
    self.flags = flags
    self.size = size
    self.modtime = modtime
    self.owner = owner
    self.group = group


class FakeURL(object):
  def __init__(self, url):
    self.url = url

  def is_valid(self):
    return True


class FakeParsedURL(FakeURL):
  def __init__(self, url):
    super(FakeParsedURL, self).__init__(url)
    self.protocol, rest = url.split('://', 1)
    parts = rest.split('/', 1)
    self.hostid = parts[0]
    if len(parts) == 1:
      self.path_with_params = '/'
    else:
      self.path_with_params = '/' + parts[1].lstrip('/')


class FakeListEntry(object):
  def __init__(self, name, statinfo):
    self.name = name
    self.statinfo = statinfo


class FakeVectorReadBuffer(object):
  def __init__(self, buffer):
    self.buffer = buffer


class FakeXRootDClient(object):
  def __init__(self, url):
    self.url = FakeURL(url)
    self.calls = []
    self.stats = {
      '/data/file.root': FakeStatInfo(
        StatInfoFlags.IS_READABLE | StatInfoFlags.IS_WRITABLE,
        size=10,
        modtime=123,
        owner='alice',
        group='analysis',
      ),
      '/data/subdir': FakeStatInfo(
        StatInfoFlags.IS_DIR | StatInfoFlags.IS_READABLE,
        size=0,
        modtime=124,
      ),
      '/data/blocked': FakeStatInfo(
        StatInfoFlags.IS_DIR | StatInfoFlags.IS_READABLE,
        size=0,
        modtime=125,
      ),
    }
    self.listings = {
      '/data': [
        FakeListEntry('file.root', self.stats['/data/file.root']),
        FakeListEntry('subdir', self.stats['/data/subdir']),
      ],
    }
    self.dirlist_errors = {
      '/data/blocked': FakeStatus(
        ok=False,
        code=XRootDStatus.errAuthFailed,
        message='Permission denied',
      ),
    }

  def _finish(self, callback, status, response):
    if callback:
      callback(status, response, [])
      return FakeStatus()
    return status, response

  def _not_found(self, callback):
    status = FakeStatus(
      ok=False,
      code=XRootDStatus.errNotFound,
      errno_value=errno.ENOENT,
      message='No such file or directory',
    )
    return self._finish(callback, status, None)

  def stat(self, path, timeout=0, callback=None):
    self.calls.append(('stat', path, timeout))
    if path not in self.stats:
      return self._not_found(callback)
    return self._finish(callback, FakeStatus(), self.stats[path])

  def dirlist(self, path, flags=0, timeout=0, callback=None):
    self.calls.append(('dirlist', path, flags, timeout))
    if path in self.dirlist_errors:
      return self._finish(callback, self.dirlist_errors[path], None)
    if path not in self.listings:
      return self._not_found(callback)
    return self._finish(callback, FakeStatus(), self.listings[path])

  def mkdir(self, path, flags=0, mode=0, timeout=0, callback=None):
    self.calls.append(('mkdir', path, flags, mode, timeout))
    return self._finish(callback, FakeStatus(), None)

  def rm(self, path, timeout=0, callback=None):
    self.calls.append(('rm', path, timeout))
    return self._finish(callback, FakeStatus(), None)

  def rmdir(self, path, timeout=0, callback=None):
    self.calls.append(('rmdir', path, timeout))
    return self._finish(callback, FakeStatus(), None)

  def mv(self, source, dest, timeout=0, callback=None):
    self.calls.append(('mv', source, dest, timeout))
    return self._finish(callback, FakeStatus(), None)

  def chmod(self, path, mode, timeout=0, callback=None):
    self.calls.append(('chmod', path, mode, timeout))
    return self._finish(callback, FakeStatus(), None)

  def query(self, querycode, arg, timeout=0, callback=None):
    self.calls.append(('query', querycode, arg, timeout))
    return self._finish(callback, FakeStatus(), b'adler32 deadbeef')

  def copy(self, source, target, force=False):
    self.calls.append(('copy', source, target, force))
    return FakeStatus(), None


class FakeXRootDFile(object):
  def __init__(self, data):
    self.data = data
    self.calls = []

  def _finish(self, callback, status, response):
    if callback:
      callback(status, response, [])
      return FakeStatus()
    return status, response

  def open(self, url, flags=0, mode=0, timeout=0, callback=None):
    self.calls.append(('open', url, flags, mode, timeout))
    return self._finish(callback, FakeStatus(), None)

  def read(self, offset=0, size=0, timeout=0, callback=None):
    self.calls.append(('read', offset, size, timeout))
    data = self.data[offset:] if size == 0 else self.data[offset:offset + size]
    return self._finish(callback, FakeStatus(), data)

  def vector_read(self, chunks, timeout=0, callback=None):
    self.calls.append(('vector_read', chunks, timeout))
    buffers = [
      FakeVectorReadBuffer(self.data[offset:offset + size])
      for offset, size in chunks
    ]
    return self._finish(callback, FakeStatus(), buffers)

  def write(self, data, offset=0, size=0, timeout=0, callback=None):
    self.calls.append(('write', data, offset, size, timeout))
    data = bytes(data)
    size = len(data) if size == 0 else size
    end = offset + size
    if end > len(self.data):
      self.data += b'\x00' * (end - len(self.data))
    self.data = self.data[:offset] + data[:size] + self.data[end:]
    return self._finish(callback, FakeStatus(), None)

  def sync(self, timeout=0, callback=None):
    self.calls.append(('sync', timeout))
    return self._finish(callback, FakeStatus(), None)

  def close(self, timeout=0, callback=None):
    self.calls.append(('close', timeout))
    return self._finish(callback, FakeStatus(), None)


@pytest.fixture
def fake_xrootd_client(monkeypatch):
  filesystems = []
  files = []

  def filesystem_factory(url):
    fs = FakeXRootDClient(url)
    filesystems.append(fs)
    return fs

  def file_factory():
    file_obj = FakeXRootDFile(b'0123456789')
    files.append(file_obj)
    return file_obj

  monkeypatch.setattr(fsspec_module.client, 'FileSystem', filesystem_factory)
  monkeypatch.setattr(fsspec_module.client, 'File', file_factory)
  monkeypatch.setattr(fsspec_module.client, 'URL', FakeParsedURL)
  return {'filesystems': filesystems, 'files': files}


def test_fsspec_url_parsing(fake_xrootd_client):
  fs = XRootDFileSystem('example.org:1094', skip_instance_cache=True)

  assert XRootDFileSystem.protocol == SUPPORTED_PROTOCOLS
  assert XRootDFileSystem._get_kwargs_from_urls(
    'root://user:pass@example.org:1094//store/data.root'
  ) == {'hostid': 'user:pass@example.org:1094', 'xrootd_protocol': 'root'}
  assert fs._strip_protocol(
    'root://example.org:1094//store/data.root'
  ) == '/store/data.root'
  assert fs.unstrip_protocol('/store/data.root') == (
    'root://example.org:1094//store/data.root'
  )


@pytest.mark.parametrize('protocol', SUPPORTED_PROTOCOLS)
def test_fsspec_protocol_aliases(protocol, fake_xrootd_client):
  url = '%s://example.org:1094//store/data.root' % protocol
  fs = XRootDFileSystem(
    'example.org:1094',
    protocol=protocol,
    skip_instance_cache=True,
  )

  assert XRootDFileSystem._get_kwargs_from_urls(url) == {
    'hostid': 'example.org:1094',
    'xrootd_protocol': protocol,
  }
  assert fs._strip_protocol(url) == '/store/data.root'
  assert fs.unstrip_protocol('/store/data.root') == url


@pytest.mark.parametrize('protocol', SUPPORTED_PROTOCOLS)
def test_fsspec_registry_url_parsing(protocol, fake_xrootd_client):
  url = '%s://example.org:1094//store/data.root' % protocol
  fsspec.register_implementation(protocol, XRootDFileSystem, clobber=True)

  fs, path = fsspec.core.url_to_fs(url)

  assert isinstance(fs, XRootDFileSystem)
  assert fs.hostid == 'example.org:1094'
  assert fs.protocol_name == protocol
  assert path == '/store/data.root'


def test_fsspec_async_methods_are_coroutines():
  async_methods = (
    '_info',
    '_ls',
    '_cat_file',
    '_get_file',
    '_mkdir',
    '_makedirs',
    '_rm_file',
    '_rmdir',
    '_mv_file',
    '_cp_file',
    '_cat_ranges',
    '_modified',
    '_checksum',
    '_chmod',
  )
  for name in async_methods:
    assert inspect.iscoroutinefunction(getattr(XRootDFileSystem, name))


def test_fsspec_rejects_unsupported_protocol():
  with pytest.raises(ValueError):
    XRootDFileSystem('example.org:1094', protocol='http')


def test_fsspec_info_from_stat_regular_file():
  info = _info_from_stat(
    '/store/data.root',
    FakeStatInfo(StatInfoFlags.IS_READABLE, size=10, modtime=123,
                 owner='alice', group='analysis'),
  )

  assert info['name'] == '/store/data.root'
  assert info['size'] == 10
  assert info['type'] == 'file'
  assert info['mtime'] == 123
  assert info['mode'] & 0o444
  assert info['uid'] == 'alice'
  assert info['gid'] == 'analysis'
  assert info['owner'] == 'alice'
  assert info['group'] == 'analysis'


def test_fsspec_info_from_stat_directory():
  info = _info_from_stat(
    '/store',
    FakeStatInfo(StatInfoFlags.IS_DIR | StatInfoFlags.IS_READABLE),
  )

  assert info['type'] == 'directory'
  assert info['mode'] & 0o111


def test_fsspec_status_not_found_maps_to_file_not_found():
  status = FakeStatus(
    ok=False,
    code=XRootDStatus.errNotFound,
    errno_value=errno.ENOENT,
    message='No such file or directory',
  )

  with pytest.raises(FileNotFoundError):
    _raise_if_error(status, 'stat', '/missing')


def test_fsspec_sync_filesystem_methods(fake_xrootd_client):
  fs = XRootDFileSystem(
    'example.org:1094',
    protocol='xroots',
    skip_instance_cache=True,
  )
  client = fake_xrootd_client['filesystems'][0]

  assert client.url.url == 'xroots://example.org:1094'
  assert fs.info('/data/file.root')['size'] == 10
  assert fs.info('/data/file.root')['uid'] == 'alice'
  assert fs.info('/data/file.root')['gid'] == 'analysis'
  assert fs.ls('/data', detail=False) == ['file.root', 'subdir']
  assert fs.ls('/data')[1]['type'] == 'directory'

  fs.mkdir('/data/new', create_parents=True)
  fs.rm_file('/data/file.root')
  fs.rmdir('/data/subdir')
  fs.mv('/data/file.root', '/data/file2.root')
  fs.cp_file('/data/file.root', '/data/file3.root', force=True)
  fs.chmod('/data/file.root', 0o600)

  assert ('dirlist', '/data', DirListFlags.STAT, 0) in client.calls
  assert fs.modified('/data/file.root') == 123
  assert fs.checksum('/data/file.root') == ('adler32', 'deadbeef')
  assert ('query', QueryCode.CHECKSUM, '/data/file.root', 0) in client.calls
  assert ('mkdir', '/data/new', MkDirFlags.MAKEPATH, 0, 0) in client.calls
  assert ('rm', '/data/file.root', 0) in client.calls
  assert ('rmdir', '/data/subdir', 0) in client.calls
  assert ('mv', '/data/file.root', '/data/file2.root', 0) in client.calls
  assert ('copy',
          'xroots://example.org:1094//data/file.root',
          'xroots://example.org:1094//data/file3.root',
          True) in client.calls
  assert ('chmod', '/data/file.root', 0o600, 0) in client.calls


def test_fsspec_sync_makedirs_existing_paths(fake_xrootd_client):
  fs = XRootDFileSystem(
    'example.org:1094',
    skip_instance_cache=True,
  )

  fs.makedirs('/data/subdir', exist_ok=True)

  with pytest.raises(FileExistsError):
    fs.makedirs('/data/subdir', exist_ok=False)
  with pytest.raises(FileExistsError):
    fs.makedirs('/data/file.root', exist_ok=True)


def test_fsspec_sync_ls_does_not_hide_directory_listing_errors(
    fake_xrootd_client):
  fs = XRootDFileSystem(
    'example.org:1094',
    skip_instance_cache=True,
  )

  with pytest.raises(OSError):
    fs.ls('/data/blocked')


def test_fsspec_sync_cat_file_reads_requested_range(fake_xrootd_client):
  fs = XRootDFileSystem(
    'example.org:1094',
    protocol='roots',
    skip_instance_cache=True,
  )

  assert fs.cat_file('/data/file.root', start=2, end=6) == b'2345'

  file_obj = fake_xrootd_client['files'][0]
  assert ('open',
          'roots://example.org:1094//data/file.root',
          OpenFlags.READ,
          0,
          0) in file_obj.calls
  assert ('read', 2, 4, 0) in file_obj.calls
  assert ('close', 0) in file_obj.calls


def test_fsspec_async_get_file_downloads_to_local_path(fake_xrootd_client,
                                                       tmp_path):
  async def run():
    fs = XRootDFileSystem(
      'example.org:1094',
      protocol='root',
      asynchronous=True,
      skip_instance_cache=True,
    )
    target = tmp_path / 'downloaded.root'

    await fs._get_file('/data/file.root', str(target))

    assert target.read_bytes() == b'0123456789'
    file_obj = fake_xrootd_client['files'][0]
    assert ('read', 0, 0, 0) in file_obj.calls
    assert ('close', 0) in file_obj.calls

  asyncio.run(run())


def test_fsspec_random_access_mode_supports_uproot_sink(fake_xrootd_client):
  fs = XRootDFileSystem(
    'example.org:1094',
    protocol='root',
    skip_instance_cache=True,
  )

  with fs.open('/data/file.root', mode='r+b') as file_obj:
    assert file_obj.read(4) == b'0123'
    assert file_obj.seek(5) == 5
    assert file_obj.write(b'abc') == 3
    assert file_obj.seek(-5, 2) == 5
    assert file_obj.read(3) == b'abc'

  xrootd_file = fake_xrootd_client['files'][0]
  assert ('open',
          'root://example.org:1094//data/file.root',
          OpenFlags.UPDATE,
          DEFAULT_ACCESS_MODE,
          0) in xrootd_file.calls
  assert ('write', b'abc', 5, 3, 0) in xrootd_file.calls
  assert ('sync', 0) in xrootd_file.calls
  assert ('close', 0) in xrootd_file.calls


def test_fsspec_sync_cat_ranges_uses_vector_read(fake_xrootd_client):
  fs = XRootDFileSystem(
    'example.org:1094',
    protocol='root',
    skip_instance_cache=True,
  )

  assert fs.cat_ranges(
    ['/data/file.root', '/data/file.root'],
    [1, 5],
    [4, 9],
  ) == [b'123', b'5678']

  file_obj = fake_xrootd_client['files'][0]
  assert ('vector_read', [(1, 3), (5, 4)], 0) in file_obj.calls
  assert ('close', 0) in file_obj.calls


def test_fsspec_missing_sync_info_raises_file_not_found(fake_xrootd_client):
  fs = XRootDFileSystem(
    'example.org:1094',
    skip_instance_cache=True,
  )

  with pytest.raises(FileNotFoundError):
    fs.info('/missing')


def test_fsspec_async_filesystem_methods(fake_xrootd_client):
  async def run():
    fs = XRootDFileSystem(
      'example.org:1094',
      protocol='xroot',
      asynchronous=True,
      skip_instance_cache=True,
    )
    client = fake_xrootd_client['filesystems'][0]

    assert (await fs._info('/data/file.root'))['size'] == 10
    assert await fs._modified('/data/file.root') == 123
    assert await fs._checksum('/data/file.root') == ('adler32', 'deadbeef')
    assert await fs._ls('/data', detail=False) == ['file.root', 'subdir']
    assert await fs._cat_file('/data/file.root', start=1, end=4) == b'123'
    assert await fs._cat_ranges(
      ['/data/file.root', '/data/file.root'],
      [0, 6],
      [2, 10],
    ) == [b'01', b'6789']
    await fs._mkdir('/data/new', create_parents=False)
    await fs._rm_file('/data/file.root')
    await fs._rmdir('/data/subdir')
    await fs._mv_file('/data/file.root', '/data/file2.root')
    await fs._cp_file('/data/file.root', '/data/file3.root', force=True)
    await fs._chmod('/data/file.root', 0o640)

    assert ('dirlist', '/data', DirListFlags.STAT, 0) in client.calls
    assert ('query', QueryCode.CHECKSUM, '/data/file.root', 0) in client.calls
    assert ('mkdir', '/data/new', MkDirFlags.NONE, 0, 0) in client.calls
    assert ('rm', '/data/file.root', 0) in client.calls
    assert ('rmdir', '/data/subdir', 0) in client.calls
    assert ('mv', '/data/file.root', '/data/file2.root', 0) in client.calls
    assert ('copy',
            'xroot://example.org:1094//data/file.root',
            'xroot://example.org:1094//data/file3.root',
            True) in client.calls
    assert ('chmod', '/data/file.root', 0o640, 0) in client.calls

  asyncio.run(run())


def test_fsspec_async_makedirs_existing_paths(fake_xrootd_client):
  async def run():
    fs = XRootDFileSystem(
      'example.org:1094',
      asynchronous=True,
      skip_instance_cache=True,
    )

    await fs._makedirs('/data/subdir', exist_ok=True)

    with pytest.raises(FileExistsError):
      await fs._makedirs('/data/subdir', exist_ok=False)
    with pytest.raises(FileExistsError):
      await fs._makedirs('/data/file.root', exist_ok=True)

  asyncio.run(run())


def test_fsspec_async_ls_does_not_hide_directory_listing_errors(
    fake_xrootd_client):
  async def run():
    fs = XRootDFileSystem(
      'example.org:1094',
      asynchronous=True,
      skip_instance_cache=True,
    )

    with pytest.raises(OSError):
      await fs._ls('/data/blocked')

  asyncio.run(run())


def test_fsspec_missing_async_info_raises_file_not_found(fake_xrootd_client):
  async def run():
    fs = XRootDFileSystem(
      'example.org:1094',
      asynchronous=True,
      skip_instance_cache=True,
    )

    with pytest.raises(FileNotFoundError):
      await fs._info('/missing')

  asyncio.run(run())
