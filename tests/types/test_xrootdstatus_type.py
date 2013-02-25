import pytest
from XRootD import client

def test_creation():
    s = client.XRootDStatus(0, 0, 0, '')
    assert s is not None
    with pytest.raises(TypeError):
        s = client.XRootDStatus()

def test_deletion():
    s = client.XRootDStatus(0, 0, 0, '')
    del s
    with pytest.raises(UnboundLocalError):
        assert s

def test_getters():
    s = client.XRootDStatus(1, 2, 3, 'spam')
    assert s.status == 1
    assert s.code == 2
    assert s.errNo == 3
    assert s.GetErrorMessage() == 'spam'
    assert s.GetShellCode() == 50
    assert s.IsError() == True
    assert s.IsFatal() == False
    assert s.IsOK() == False