from XRootD import client
import pytest

def test_valid_url():
    c = client.Client('root://localhost')
    assert  c.url.IsValid()
    
def test_invalid_url():
    c = client.Client('root://')
    print c.url
    assert c.url.IsValid() == False
    
def test_args():
    c = client.Client(url='root://localhost')
    assert c
    
    with pytest.raises(TypeError):
        c = client.Client(foo='root://localhost')
        
    with pytest.raises(TypeError):
        c = client.Client(path='root://localhost', foo='bar')


