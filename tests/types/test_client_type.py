import pytest
from XRootD import client

# Test creation/deletion of custom types and their attributes
# Test default and custom values of custom types

def test_creation():
    c = client.Client("root://localhost")
    assert c.url is not None

def test_deletion():
    c = client.Client("root://localhost")
    del c
    with pytest.raises(UnboundLocalError):
        assert c