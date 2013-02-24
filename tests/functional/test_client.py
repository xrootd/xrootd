from XRootD import client
import pytest

def test_valid_url():
    c = client.Client('root://localhost')
    status, response = c.stat('/tmp')
    assert status.IsOK()
    
def test_invalid_url():
    c = client.Client('foo://bar')
    status, response = c.stat('/baz')
    assert status.IsError()