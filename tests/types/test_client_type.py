from XRootD import client
import pytest

def test_creation():
    c = client.Client("root://localhost")
    assert c.url is not None

def test_deletion():
    c = client.Client("root://localhost")
    del c
    with pytest.raises(UnboundLocalError):
        assert c