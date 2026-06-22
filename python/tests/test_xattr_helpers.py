from XRootD import client
from XRootD.client.utils import AsyncResponseHandler


OK_STATUS = {
  'ok': True,
  'message': 'OK'
}

ERR_STATUS = {
  'ok': False,
  'message': 'xattr failed'
}


class FakeFileSystemBackend(object):
  def __init__(self, list_response=None, get_response=None):
    self.list_response = list_response or []
    self.get_response = get_response or []
    self.calls = []

  def list_xattr(self, path, timeout=0, callback=None):
    self.calls.append(('list_xattr', path, timeout))
    if callback:
      callback(OK_STATUS, self.list_response)
      return OK_STATUS
    return OK_STATUS, self.list_response

  def get_xattr(self, path, attrs, timeout=0, callback=None):
    self.calls.append(('get_xattr', path, attrs, timeout))
    if callback:
      callback(OK_STATUS, self.get_response)
      return OK_STATUS
    return OK_STATUS, self.get_response


class FakeFileBackend(object):
  def __init__(self, list_response=None, get_response=None):
    self.list_response = list_response or []
    self.get_response = get_response or []
    self.calls = []

  def list_xattr(self, timeout=0, callback=None):
    self.calls.append(('list_xattr', timeout))
    if callback:
      callback(OK_STATUS, self.list_response)
      return OK_STATUS
    return OK_STATUS, self.list_response

  def get_xattr(self, attrs, timeout=0, callback=None):
    self.calls.append(('get_xattr', attrs, timeout))
    if callback:
      callback(OK_STATUS, self.get_response)
      return OK_STATUS
    return OK_STATUS, self.get_response

  def is_open(self):
    return False


def filesystem(backend):
  fs = object.__new__(client.FileSystem)
  fs._FileSystem__fs = backend
  return fs


def file(backend):
  f = object.__new__(client.File)
  f._File__file = backend
  return f


def test_filesystem_xattrs_returns_mapping():
  backend = FakeFileSystemBackend(
    list_response=[('first', 'one', OK_STATUS), ('second', 'two', OK_STATUS)]
  )
  fs = filesystem(backend)

  status, response = fs.xattrs('/tmp/data')

  assert status.ok
  assert response == {'first': 'one', 'second': 'two'}
  assert backend.calls == [('list_xattr', '/tmp/data', 0)]


def test_filesystem_xattr_returns_single_value():
  backend = FakeFileSystemBackend(
    get_response=[('spacetoken.description?test', 'quota-info', OK_STATUS)]
  )
  fs = filesystem(backend)

  status, response = fs.xattr('/tmp/data', 'spacetoken.description?test',
                              timeout=15)

  assert status.ok
  assert response == 'quota-info'
  assert backend.calls == [
    ('get_xattr', '/tmp/data', ['spacetoken.description?test'], 15)
  ]


def test_xattrs_returns_per_attribute_failure():
  backend = FakeFileSystemBackend(
    list_response=[('broken', '', ERR_STATUS)]
  )
  fs = filesystem(backend)

  status, response = fs.xattrs('/tmp/data')

  assert not status.ok
  assert status.message == 'xattr failed'
  assert response is None


def test_filesystem_xattrs_callback_returns_mapping():
  backend = FakeFileSystemBackend(
    list_response=[('first', 'one', OK_STATUS)]
  )
  fs = filesystem(backend)
  handler = AsyncResponseHandler()

  status = fs.xattrs('/tmp/data', callback=handler)
  callback_status, response, hostlist = handler.wait()

  assert status.ok
  assert callback_status.ok
  assert response == {'first': 'one'}
  assert len(hostlist.hosts) == 0


def test_file_xattrs_returns_mapping():
  backend = FakeFileBackend(
    list_response=[('first', 'one', OK_STATUS), ('second', 'two', OK_STATUS)]
  )
  f = file(backend)

  status, response = f.xattrs()

  assert status.ok
  assert response == {'first': 'one', 'second': 'two'}
  assert backend.calls == [('list_xattr', 0)]


def test_file_xattr_callback_returns_single_value():
  backend = FakeFileBackend(
    get_response=[('first', 'one', OK_STATUS)]
  )
  f = file(backend)
  handler = AsyncResponseHandler()

  status = f.xattr('first', timeout=5, callback=handler)
  callback_status, response, hostlist = handler.wait()

  assert status.ok
  assert callback_status.ok
  assert response == 'one'
  assert len(hostlist.hosts) == 0
  assert backend.calls == [('get_xattr', ['first'], 5)]
