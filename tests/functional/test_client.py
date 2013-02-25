from XRootD import client
import pytest, time

def test_valid_url():
    c = client.Client('root://localhost')
    status, response = c.stat('/tmp')
    assert status.IsOK()
    assert response != None
    
def test_invalid_url():
    c = client.Client('foo://bar')
    status, response = c.stat('/baz')
    assert status.IsError()
    assert response == None
    
def test_stat_sync():
    c = client.Client("root://localhost")
    status, response = c.stat("/tmp")
    assert status
    assert response

