from XRootD import client
from XRootD.handlers import AsyncResponseHandler
from XRootD.enums import OpenFlags, QueryCode, MkDirFlags, AccessMode
import pytest

def test_filesystem():
    c = client.Client("root://localhost")
    
    funcspecs = [(c.locate,     ('/tmp', OpenFlags.REFRESH), True),
                 (c.deeplocate, ('/tmp', OpenFlags.REFRESH), True),
                #(c.mv,         ('/tmp/spam', '/tmp/ham'), False),
                 (c.query,      (QueryCode.SPACE, '/tmp'), True),
                 (c.truncate,   ('/tmp/spam', 1000), False),
                #(c.rm,         ('/tmp/ham'), False),
                #(c.mkdir,      ('/tmp/1/2', MkDirFlags.MAKEPATH), False),
                 (c.rmdir,      ('/tmp/somedir',), False),
                 (c.chmod,      ('/tmp/eggs', AccessMode.UR), False),
                 (c.ping,       (), False),
                 (c.stat,       ('/tmp',), True),
                 (c.statvfs,    ('/tmp',), True),
                 (c.protocol,   (), True),
                 (c.dirlist,    ('/tmp',), True),
                 (c.sendinfo,   (), False),
                 (c.prepare,    (), True),
                 ]

    for func, args, hasReturnObject in funcspecs:
        sync (func, args, hasReturnObject)
        async(func, args, hasReturnObject)

def sync(func, args, hasReturnObject):
    c = client.Client("root://localhost")
    status, response = func(*args)
    print status
    assert status['isOK']
    if hasReturnObject:
        print response
        assert response
    
def async(func, args, hasReturnObject):
    c = client.Client("root://localhost")
    handler = AsyncResponseHandler()
    status = func(*args, callback=handler)
    print status
    assert status['isOK']
    status, response, hostList = handler.waitFor()
    #assert 0
    assert status['isOK']
    if hasReturnObject:
        print response
        assert response
    #assert hostList
    
def test_args():
    c = client.Client("root://localhost")
    status, response = c.locate(path="/tmp", flags=0, timeout=1)
    assert status
    assert response
    
    with pytest.raises(TypeError):
        c.locate(path="/tmp")
        
    with pytest.raises(TypeError):
        c.locate(path="/tmp", foo=1)
        
    with pytest.raises(TypeError):
        c.locate(foo="/tmp")
        
    with pytest.raises(TypeError):
        c.locate(path="/tmp", flags=1, foo=0)