from XRootD import client
import pytest, sys
from env import *

def test_creation():
    c = client.Client(SERVER_URL)
    assert c.url is not None

def test_deletion():
    c = client.Client(SERVER_URL)
    del c
    
    if sys.hexversion > 0x03000000:
        pytest.raises(UnboundLocalError, 'assert c')
    else:
        pytest.raises(NameError, 'assert c')