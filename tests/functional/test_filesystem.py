from XRootD import client, handlers
import pytest

def test_filesystem():
    c = client.Client("root://localhost")
    
    funcargs = {c.locate    : ('/tmp', 0),
                c.ping      : (), 
                c.stat      : ('/tmp')}

    for func, args in funcargs.iteritems():
        sync(func, args)
        async(func, args)

def sync(func, args):
    c = client.Client("root://localhost")
    status, response = func(*args)
    assert status
    assert response
    
def async(func, args):
    c = client.Client("root://localhost")
    handler = handlers.AsyncResponseHandler()
    status = func(*args, callback=handler)
    assert status
    status, response, hostList = handler.waitFor()
    assert status
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