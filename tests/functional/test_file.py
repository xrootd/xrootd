from XRootD import client
from XRootD.handlers import AsyncResponseHandler
from XRootD.enums import OpenFlags

import pytest
import sys

def test_write():
    f = client.File()
    buffer = 'eggs and ham\n'

    pytest.raises(ValueError, "f.write(buffer)")

    status, response = f.open('root://localhost//tmp/spam', OpenFlags.DELETE)
    assert status['isOK']

    status, response = f.write(buffer)
    assert status['isOK']

    status, response = f.read()
    assert status['isOK']
    assert len(response) == len(buffer)

        # Test write offsets and sizes
        
    f.close()

def test_read():
    f = client.File()
    pytest.raises(ValueError, 'f.read()')

    status, response = f.open('root://localhost//tmp/spam', OpenFlags.READ)
    assert status['isOK']

    status, response = f.read()
    assert status['isOK']

    assert response
    
    f.readline(offset=0, size=1024)
    assert 0 # TODO add readline() params

    # Test read offsets and sizes
    f.close()

def test_vector_read():
    v = [(0, 100), (101, 200), (201, 200)]
    
    f = client.File()
    pytest.raises(ValueError, 'f.vector_read()')
        

    status, response = f.open('root://localhost//tmp/spam', OpenFlags.READ)
    assert status['isOK']
    status, response = f.vector_read(chunks=v)
    assert status['isOK'] == False
    assert not response
    f.close()

    f = client.File()
    status, response = f.open('root://localhost//tmp/xrootd.tgz', OpenFlags.READ)
    print status
    assert status['isOK']
    status, response = f.vector_read(chunks=v)
    assert status['isOK']
    assert response
    f.close()

def test_stat():
    f = client.File()

    pytest.raises(ValueError, 'f.stat()')
        

    status, response = f.open('root://localhost//tmp/spam')
    assert status['isOK']

    status, response = f.stat()
    assert status['isOK']
    assert response['size']
    f.close()

def test_sync():
    f = client.File()
    
    pytest.raises(ValueError, 'f.sync()')

    status, response = f.open('root://localhost//tmp/spam')
    assert status['isOK']
    
    status, response = f.sync()
    assert status['isOK']
    f.close()

def test_truncate():
    f = client.File()

    pytest.raises(ValueError, 'f.truncate()')
        
    status, response = f.open('root://localhost//tmp/spam', OpenFlags.UPDATE)
    assert status['isOK']

    status, response = f.truncate(size=10000) # TODO: Fix EINVAL
    print status
    assert status['isOK']
    f.close()

def test_misc():
    f = client.File()
    assert not f.is_open()
    
    status, response = f.open('root://localhost//tmp/spam', OpenFlags.READ)
    assert status['isOK']
    assert f.is_open()

    pytest.raises(TypeError, "f.enable_read_recovery(enable='foo')")
        

    f.enable_read_recovery(enable=True)

    pytest.raises(TypeError, "f.enable_write_recovery(enable='foo')")

    f.enable_write_recovery(enable=True)

    assert f.get_data_server()

    pytest.raises(TypeError, 'f.get_data_server("invalid_arg")')
        

    # testing context manager
    f.close()
    assert not f.is_open()
