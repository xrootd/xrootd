from XRootD import client
from XRootD.handlers import AsyncResponseHandler
from XRootD.enums import OpenFlags

import pytest

def test_file():
    with client.File() as f:
        assert not f.is_open()
        status, response = f.open('root://localhost//tmp/spam', OpenFlags.READ)
        assert status['isOK']
        assert f.is_open()

        with pytest.raises(TypeError):
            f.enable_read_recovery(enable="foo")
            
        f.enable_read_recovery(enable=True)

        with pytest.raises(TypeError):
            f.enable_write_recovery(enable="foo")
            
        f.enable_write_recovery(enable=True)

        assert f.get_data_server()

        with pytest.raises(TypeError):
            f.get_data_server("invalid_arg")

    # testing context manager
    assert not f.is_open()

def test_read():
    with client.File() as f:
        with pytest.raises(ValueError):
            f.read()

        status, response = f.open('root://localhost//tmp/spam', OpenFlags.READ)
        assert status['isOK']

        status, response = f.read()
        assert status['isOK']

        assert response

        # Test read offsets and sizes

def test_vector_read():
    with client.File() as f:
        with pytest.raises(ValueError):
            f.vector_read()  # TODO: Implement this

        status, response = f.open('root://localhost//tmp/spam', OpenFlags.READ)
        assert status['isOK']
        
        status, response = f.vector_read()
        assert status['isOK']

        assert response
    
def test_write():
    with client.File() as f:
        buffer = 'eggs and ham'

        with pytest.raises(ValueError):
            f.write(buffer)

        status, response = f.open('root://localhost//tmp/spam', OpenFlags.DELETE)
        assert status['isOK']

        status, response = f.write(buffer)
        assert status['isOK']

        status, response = f.read()
        assert status['isOK']
        assert len(response) == len(buffer)

        # Test write offsets and sizes

def test_stat():
    with client.File() as f:
        
        with pytest.raises(ValueError):
            f.stat()

        status, response = f.open('root://localhost//tmp/spam')
        assert status['isOK']
        
        # status, response = f.stat()
        assert 0  # TODO: Fix stat() with force=False
        assert status['isOK']
        assert response['size']

def test_sync():
    with client.File() as f:
        
        with pytest.raises(ValueError):
            f.sync()
            
        status, response = f.open('root://localhost//tmp/spam')
        assert status['isOK']
        
        status, response = f.sync()
        assert status['isOK']

def test_truncate():
    with client.File() as f:
        
        with pytest.raises(ValueError):
            f.truncate()
            
        status, response = f.open('root://localhost//tmp/spam')
        assert status['isOK']
        
        status, response = f.truncate(size=1000) # TODO: Fix EINVAL
        print status
        assert status['isOK']
