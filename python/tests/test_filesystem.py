from XRootD import client
from XRootD.client.utils import AsyncResponseHandler
from XRootD.client.flags import OpenFlags, QueryCode, MkDirFlags, AccessMode, \
                                 DirListFlags, PrepareFlags
from env import *
import pytest
import sys
import os
import inspect

def test_filesystem():
  c = client.FileSystem(SERVER_URL)

  funcspecs = [(c.locate,     ('/tmp', OpenFlags.REFRESH), True),
               (c.deeplocate, ('/tmp', OpenFlags.REFRESH), True),
               (c.query,      (QueryCode.SPACE, '/tmp'), True),
               (c.truncate,   ('/tmp/spam', 1000), False),
               (c.mv,         ('/tmp/spam', '/tmp/ham'), False),
               (c.chmod,      ('/tmp/ham', AccessMode.UR | AccessMode.UW), False),
               (c.rm,         ('/tmp/ham',), False),
               (c.mkdir,      ('/tmp/somedir', MkDirFlags.MAKEPATH), False),
               (c.rmdir,      ('/tmp/somedir',), False),
               (c.ping,       (), False),
               (c.stat,       ('/tmp',), True),
               (c.statvfs,    ('/tmp',), True),
               (c.protocol,   (), True),
               (c.dirlist,    ('/tmp', DirListFlags.STAT), True),
               (c.sendinfo,   ('important info',), False),
               (c.prepare,    (['/tmp/foo'], PrepareFlags.STAGE), True),
               ]

  # Each pass consumes the file: it is truncated, renamed, and then removed.

  create(smallfile)

  for func, args, hasReturnObject in funcspecs:
      sync (func, args, hasReturnObject)

  create(smallfile)

  for func, args, hasReturnObject in funcspecs:
      run_async(func, args, hasReturnObject)

def create(path):
  f = client.File()
  status, response = f.open(path, OpenFlags.DELETE)
  assert status.ok
  status, response = f.close()
  assert status.ok

def sync(func, args, hasReturnObject):
  status, response = func(*args)
  print(status)
  assert status.ok
  if hasReturnObject:
      print(response)
      assert response

def run_async(func, args, hasReturnObject):
  handler = AsyncResponseHandler()
  status = func(callback=handler, *args)
  print(status)
  assert status.ok
  status, response, hostlist = handler.wait()

  assert status.ok
  if response:
      assert response

  for host in hostlist:
    assert host.url
    print(host.url)

  if hasReturnObject:
    assert response

def test_copy_sync():
  c = client.FileSystem(SERVER_URL)
  f = client.File()
  status, response = f.open(smallfile, OpenFlags.DELETE)
  assert status.ok
  
  status, response = c.copy(smallfile, '/tmp/eggs', force=True)
  assert status.ok
  
  status, response = c.copy('/tmp/nonexistent', '/tmp/eggs')
  assert not status.ok

  try:
    os.remove('/tmp/eggs')
  except OSError as __:
    pass

def test_locate_sync():
  c = client.FileSystem(SERVER_URL)
  status, response = c.locate('/tmp', OpenFlags.REFRESH)
  assert status.ok

  for item in response:
    assert item

def test_locate_async():
  c = client.FileSystem(SERVER_URL)
  handler = AsyncResponseHandler()
  response = c.locate('/tmp', OpenFlags.REFRESH, callback=handler)

  status, response, hostlist = handler.wait()
  assert status.ok

  for item in response:
    assert item

def test_deeplocate_sync():
  c = client.FileSystem(SERVER_URL)
  status, response = c.deeplocate('/tmp', OpenFlags.REFRESH)
  assert status.ok

  for item in response:
    assert item

def test_deeplocate_async():
  c = client.FileSystem(SERVER_URL)
  handler = AsyncResponseHandler()
  response = c.deeplocate('/tmp', OpenFlags.REFRESH, callback=handler)

  status, response, hostlist = handler.wait()
  assert status.ok

  for item in response:
    assert item

def test_dirlist_sync():
  c = client.FileSystem(SERVER_URL)
  status, response = c.dirlist('/tmp', DirListFlags.STAT)
  assert status.ok

  for item in response:
    assert item.name
    print(item.statinfo)
    assert item.statinfo
    assert item.hostaddr
    
  status, response = c.dirlist('invalid', DirListFlags.STAT)
  assert not status.ok

def test_dirlist_async():
  c = client.FileSystem(SERVER_URL)
  handler = AsyncResponseHandler()
  status = c.dirlist('/tmp', DirListFlags.STAT, callback=handler)
  assert status.ok
  status, response, hostlist = handler.wait()
  assert status.ok

  for h in hostlist:
    print(h.url)

  for item in response:
    assert item.name
    print(item.statinfo)
    assert item.statinfo
    assert item.hostaddr

  assert hostlist

def test_query_sync():
  c = client.FileSystem(SERVER_URL)
  status, response = c.query(QueryCode.STATS, 'a')
  assert status.ok
  assert response
  print(response)

def test_checksum_sync():
  c = client.FileSystem(SERVER_URL)
  f = client.File()
  status, response = f.open(smallfile, OpenFlags.DELETE)
  assert status.ok
  f.write(smallbuffer)
  f.close()

  status, response = c.checksum('/tmp/spam')
  assert status.ok
  assert response.algorithm
  assert response.value


def test_checksum_async_response():
  raw_status = {'ok': True, 'message': 'ok'}

  class Recorder(object):
    def query(self, querycode, path, timeout, callback):
      self.args = (querycode, path, timeout)
      callback(raw_status, b'adler32 deadbeef\0', [])
      return raw_status

  class FileSystem(object):
    def __init__(self):
      self._FileSystem__fs = Recorder()

  responses = []
  filesystem = FileSystem()
  status = client.FileSystem.checksum(
    filesystem, '/tmp/file', timeout=12,
    callback=lambda *args: responses.append(args))

  assert status.ok
  assert filesystem._FileSystem__fs.args == (
    QueryCode.CHECKSUM, '/tmp/file', 12)
  assert len(responses) == 1
  assert responses[0][0].ok
  assert responses[0][1].algorithm == 'adler32'
  assert responses[0][1].value == 'deadbeef'

def test_query_async():
  c = client.FileSystem(SERVER_URL)
  handler = AsyncResponseHandler()
  status = c.query(QueryCode.STATS, 'a', callback=handler)
  assert status.ok

  status, response, hostlist = handler.wait()
  assert status.ok
  assert response
  print(response)
  
def test_mkdir_flags():
  c = client.FileSystem(SERVER_URL)
  status, response = c.mkdir('/tmp/dir1/dir2', MkDirFlags.MAKEPATH)
  assert status.ok
  c.rm('/tmp/dir1/dir2')
  c.rm('/tmp/dir1')

def test_mkdir_p():
  c = client.FileSystem(SERVER_URL)
  status, response = c.mkdir_p('/tmp/dir1/dir2', flags=MkDirFlags.NONE)
  assert status.ok
  c.rm('/tmp/dir1/dir2')
  c.rm('/tmp/dir1')


def test_mkdir_p_preserves_flags():
  class Recorder(object):
    def mkdir(self, path, flags=0, mode=0, timeout=0, callback=None):
      self.args = (path, flags, mode, timeout, callback)
      return 'result'

  recorder = Recorder()
  callback = object()
  result = client.FileSystem.mkdir_p(recorder, '/tmp/dir', flags=4, mode=0o750,
                                     timeout=12, callback=callback)

  assert result == 'result'
  assert recorder.args == ('/tmp/dir', 4 | MkDirFlags.MAKEPATH, 0o750, 12,
                           callback)

def test_args():
  c = client.FileSystem(url=SERVER_URL)
  assert c is not None

  pytest.raises(TypeError, client.FileSystem, foo='root://localhost')
  pytest.raises(TypeError, client.FileSystem, path='root://localhost', foo='bar')

def test_creation():
  c = client.FileSystem(SERVER_URL)
  assert c.url is not None

def test_context_manager():
  with client.FileSystem(SERVER_URL) as c:
    assert c.url is not None

  with pytest.raises(RuntimeError):
    with client.FileSystem(SERVER_URL):
      raise RuntimeError('failure')

def test_deletion():
  c = client.FileSystem(SERVER_URL)
  del c
  pytest.raises(NameError, eval, "c")
