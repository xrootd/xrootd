from XRootD import client
from XRootD.client import AsyncResponseHandler
from XRootD.enums  import OpenFlags, QueryCode, MkDirFlags, AccessMode, \
                          DirListFlags, PrepareFlags
import pytest
  
def test_filesystem():
    c = client.Client("root://localhost")
  
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
    status, response = f.open('root://localhost//tmp/spam', OpenFlags.NEW)
  
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
  
def test_args():
    c = client.Client("root://localhost")
    status, response = c.locate(path="/tmp", flags=0, timeout=1)
    assert status
    assert response
  
    pytest.raises(TypeError, 'c.locate(path="/tmp")')
    pytest.raises(TypeError, 'c.locate(path="/tmp", foo=1)')
    pytest.raises(TypeError, 'c.locate(foo="/tmp")')
    pytest.raises(TypeError, 'c.locate(path="/tmp", flags=1, foo=0)')

