from XRootD import client

def test_creation():
  assert client.Client("root://localhost") is not None

