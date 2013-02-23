import pytest
from XRootD import client

def test_creation():
  u = client.URL("root://localhost")
  assert u is not None

def test_deletion():
  u = client.URL("root://localhost")
  del u
  with pytest.raises(UnboundLocalError):
    assert u