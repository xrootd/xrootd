from XRootD import client
import pytest

def test_valid_url():
    c = client.Client('root://localhost')
    assert  c.url.is_valid()

def test_invalid_url():
    c = client.Client('root://')
    print c.url
    assert c.url.is_valid() == False

def test_args():
    c = client.Client(url='root://localhost')
    assert c

    pytest.raises(TypeError, "c = client.Client(foo='root://localhost')")

    pytest.raises(TypeError, "c = client.Client(path='root://localhost', foo='bar')")


