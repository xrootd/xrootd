import pytest
from XRootD import client

def test_creation():
  x = client.XRootDStatus(0, 0, 0, '')
  assert x is not None

def test_deletion():
  x = client.XRootDStatus(0, 0, 0, '')
  del x
  with pytest.raises(UnboundLocalError):
    assert x