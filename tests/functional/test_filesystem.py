from XRootD import client
from XRootD.handlers import AsyncResponseHandler
from XRootD.enums import OpenFlags
import pytest

def test_filesystem():
    c = client.Client("root://localhost")
    
    funcspecs = [(c.locate,     ('/tmp', OpenFlags.REFRESH), True),
                 (c.deeplocate, ('/tmp', OpenFlags.REFRESH), True),
                 (c.mv,         ('/tmp/spam', '/tmp/ham'), False),
                 (c.ping,       (), False),
                 (c.stat,       ('/tmp'), True)
                 ]

    for func, args, hasReturnObject in funcspecs:
        sync (func, args, hasReturnObject)
        async(func, args, hasReturnObject)

def sync(func, args, hasReturnObject):
    c = client.Client("root://localhost")
    status, response = func(*args)
    assert status['isOK']
    print status
    if hasReturnObject:
        assert response
    
def async(func, args, hasReturnObject):
    c = client.Client("root://localhost")
    handler = AsyncResponseHandler()
    status = func(*args, callback=handler)
    assert status['isOK']
    print status
    status, response, hostList = handler.waitFor()
    #assert 0
    assert status['isOK']
    if hasReturnObject:
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