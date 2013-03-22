from XRootD import client
import pytest, sys

def test_creation():
    c = client.Client("root://localhost")
    assert c.url is not None

def test_deletion():
    c = client.Client("root://localhost")
    del c
    
    if sys.hexversion > 0x02050000:
        pytest.raises(UnboundLocalError, 'assert c')
    else:
        pytest.raises(NameError, 'assert c')