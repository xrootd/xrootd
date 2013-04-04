from XRootD import client
from XRootD.client import AsyncResponseHandler
from XRootD.enums import OpenFlags
from env import *
 
import pytest
import sys
 
def test_write():
    f = client.File()
    buffer = 'eggs and ham\n'
 
    pytest.raises(ValueError, "f.write(buffer)")
    status, response = f.open(SERVER_URL + '/tmp/spam', OpenFlags.DELETE)
    assert status.ok
 
    status, response = f.write(buffer)
    assert status.ok
    status, response = f.read()
    assert status.ok
    assert len(response) == len(buffer)

    handler = AsyncResponseHandler()
    status = f.write(buffer, callback=handler)
    status, response, hostlist = handler.wait()
    assert status.ok
    status, response = f.read()
    assert status.ok
    assert len(response) == len(buffer)
    
    # Test write offsets and sizes
    f.close()
 
def test_read():
    f = client.File()
    pytest.raises(ValueError, 'f.read()')
 
    status, response = f.open(SERVER_URL + '/tmp/bigfile', OpenFlags.READ)
    assert status.ok
     
    status, response = f.stat(timeout=5)
    size = response.size
 
    status, response = f.read()
    assert status.ok
    assert len(response) == size
     
    f.readline()
    f.close()
 
    # Test read offsets and sizes
    f.close()
 
def test_vector_read():
    v = [(0, 100), (101, 200), (201, 200)]
     
    f = client.File()
    pytest.raises(ValueError, 'f.vector_read(v)')
 
    status, response = f.open(SERVER_URL + '/tmp/spam', OpenFlags.READ)
    assert status.ok
    status, response = f.vector_read(chunks=v)
    assert status.ok == False
    assert not response
    f.close()
 
    f = client.File()
    status, response = f.open(SERVER_URL + '/tmp/bigfile', OpenFlags.READ)
    print status
    assert status.ok
    status, response = f.vector_read(chunks=v)
    assert status.ok
    assert response
    f.close()
 
def test_stat():
    f = client.File()
 
    pytest.raises(ValueError, 'f.stat()')
         
 
    status, response = f.open(SERVER_URL + '/tmp/spam')
    assert status.ok
 
    status, response = f.stat()
    assert status.ok
    assert response.size
    f.close()
 
def test_sync():
    f = client.File()
     
    pytest.raises(ValueError, 'f.sync()')
 
    status, response = f.open(SERVER_URL + '/tmp/spam')
    assert status.ok
     
    status, response = f.sync()
    assert status.ok
    f.close()
 
def test_truncate():
    f = client.File()
 
    pytest.raises(ValueError, 'f.truncate(10000)')
     
    status, response = f.open(SERVER_URL + '/tmp/spam', OpenFlags.UPDATE)
    assert status.ok
 
    status, response = f.truncate(size=10000)
    print status
    assert status.ok
    f.close()
 
def test_misc():
    f = client.File()
    assert not f.is_open()
     
    status, response = f.open(SERVER_URL + '/tmp/spam', OpenFlags.READ)
    assert status.ok
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
