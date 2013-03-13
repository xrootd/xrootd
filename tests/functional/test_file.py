from XRootD import client
from XRootD.handlers import AsyncResponseHandler
from XRootD.enums import OpenFlags

import pytest

def test_file():
    f = client.File()
    f.open('root://localhost//tmp/spam', OpenFlags.READ)
    f.is_open()