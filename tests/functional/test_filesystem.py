from XRootD import client
from XRootD.handlers import AsyncResponseHandler
import pytest

def test_filesystem():
    c = client.Client("root://localhost")
    
    funcspecs = {c.locate    : (('/tmp', 0), True),
                 c.ping      : ((), False),
                 c.stat      : (('/tmp', 0), True)}

    for func, args in funcspecs.iteritems():
        sync (func, args[0], args[1])
        async(func, args[0], args[1])

def sync(func, args, hasReturnObject):
    c = client.Client("root://localhost")
    status, response = func(*args)
    assert status
    if hasReturnObject:
        assert response
    
def async(func, args, hasReturnObject):
    c = client.Client("root://localhost")
    handler = AsyncResponseHandler()
    status = func(*args, callback=handler)
    assert status
    status, response, hostList = handler.waitFor()
    #assert 0
    assert status
    if hasReturnObject:
        assert response
    assert hostList
    
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