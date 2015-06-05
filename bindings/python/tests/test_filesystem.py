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

  for func, args, hasReturnObject in funcspecs:
      sync (func, args, hasReturnObject)

  # Create new temp file
  f = client.File()
  status, response = f.open(smallfile, OpenFlags.NEW)

  for func, args, hasReturnObject in funcspecs:
      async(func, args, hasReturnObject)

def sync(func, args, hasReturnObject):
  status, response = func(*args)
  print status
  assert status.ok
  if hasReturnObject:
      print response
      assert response

def async(func, args, hasReturnObject):
  handler = AsyncResponseHandler()
  status = func(callback=handler, *args)
  print status
  assert status.ok
  status, response, hostlist = handler.wait()

  assert status.ok
  if response:
      assert response

  for host in hostlist:
    assert host.url
    print host.url

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
  except OSError, __:
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
    print item.statinfo
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
    print h.url

  for item in response:
    assert item.name
    print item.statinfo
    assert item.statinfo
    assert item.hostaddr

  assert hostlist

def test_query_sync():
  c = client.FileSystem(SERVER_URL)
  status, response = c.query(QueryCode.STATS, 'a')
  assert status.ok
  assert response
  print response

def test_query_async():
  c = client.FileSystem(SERVER_URL)
  handler = AsyncResponseHandler()
  status = c.query(QueryCode.STATS, 'a', callback=handler)
  assert status.ok

  status, response, hostlist = handler.wait()
  assert status.ok
  assert response
  print response
  
def test_mkdir_flags():
  c = client.FileSystem(SERVER_URL)
  status, response = c.mkdir('/tmp/dir1/dir2', MkDirFlags.MAKEPATH)
  assert status.ok
  c.rm('/tmp/dir1/dir2')
  c.rm('/tmp/dir1')
  

def test_args():
  c = client.FileSystem(url=SERVER_URL)
  assert c

  pytest.raises(TypeError, "c = client.FileSystem(foo='root://localhost')")
  pytest.raises(TypeError, "c = client.FileSystem(path='root://localhost', foo='bar')")

def test_creation():
  c = client.FileSystem(SERVER_URL)
  assert c.url is not None

def test_deletion():
  c = client.FileSystem(SERVER_URL)
  del c

  if sys.hexversion > 0x03000000:
      pytest.raises(UnboundLocalError, 'assert c')
  else:
      pytest.raises(NameError, 'assert c')

