from XRootD import client
import pytest
from env import *
 
def test_valid_url():
    c = client.Client(SERVER_URL)
    assert  c.url.is_valid()
 
def test_invalid_url():
    c = client.Client('root://')
    print c.url
    assert c.url.is_valid() == False
 
def test_args():
    c = client.Client(url=SERVER_URL)
    assert c
 
    pytest.raises(TypeError, "c = client.Client(foo='root://localhost')")
 
    pytest.raises(TypeError, "c = client.Client(path='root://localhost', foo='bar')")


