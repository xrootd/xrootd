from XRootD import client
from XRootD.client.utils import AsyncResponseHandler
from XRootD.client.enums import OpenFlags
from env import *

import pytest
import sys

smallfile = SERVER_URL + '/tmp/spam'
smallbuffer = 'eggs and ham\n'
bigfile = SERVER_URL + '/tmp/bigfile'

def test_open_close():
  f = client.File()
  pytest.raises(ValueError, "f.stat()")
  status, response = f.open(smallfile, OpenFlags.READ)
  assert status.ok
  assert f.is_open()
  
  status, response = f.close()
  assert status.ok
  assert f.is_open() == False
  pytest.raises(ValueError, "f.stat()")
  f.close()
  f.close()

def test_write_sync():
  f = client.File()
  pytest.raises(ValueError, "f.write(smallbuffer)")
  status, response = f.open(smallfile, OpenFlags.DELETE)
  assert status.ok

  status, response = f.write(smallbuffer)
  assert status.ok
  status, response = f.read()
  assert status.ok
  assert len(response) == len(smallbuffer)
  
  status, response = f.write(smallbuffer, offset=13, size=len(smallbuffer) - 2)
  assert status.ok
  status, response = f.read()
  assert status.ok
  assert len(response) == len(smallbuffer*2) - 2
  
  f.close()

def test_write_async():
  f = client.File()
  status, response = f.open(smallfile, OpenFlags.DELETE)
  assert status.ok
  
  handler = AsyncResponseHandler()
  status = f.write(smallbuffer, callback=handler)
  status, response, hostlist = handler.wait()
  assert status.ok
  status, response = f.read()
  assert status.ok
  assert len(response) == len(smallbuffer)

  f.close()

def test_read_sync():
  f = client.File()
  pytest.raises(ValueError, 'f.read()')
  status, response = f.open(bigfile, OpenFlags.READ)
  assert status.ok

  status, response = f.stat()
  size = response.size

  status, response = f.read()
  assert status.ok
  assert len(response) == size

  # Test read offsets and sizes
  f.close()
  
def test_read_async():
  f = client.File()
  status, response = f.open(bigfile, OpenFlags.READ)
  assert status.ok
  
  status, response = f.stat()
  size = response.size

  handler = AsyncResponseHandler()
  status = f.read(callback=handler)
  assert status.ok
  status, response, hostlist = handler.wait()
  assert status.ok
  assert len(response) == size
  
  f.close()
  
def test_iteration():
  f = client.File()
  f.open(smallfile, OpenFlags.DELETE)
  f.write('gre\0en\neggs\nand\nham\n')
  size = f.stat(force=True)[1].size
  
  total = 0
  for line in f:
    print '+++++ %r' % line
    total += len(line)

  assert total == size
  f.close()

  f = client.File()
  f.open(bigfile, OpenFlags.READ)
    
  size = f.stat()[1].size
  total = 0
    
  for line in f:
    total += len(line)
     
  assert total == size
  f.close()
  
def test_readline():
  f = client.File()
  f.open(smallfile, OpenFlags.DELETE)
  f.write('gre\0en\neggs\nand\nham\n')
  
  response = f.readline()
  assert response == 'gre\0en\n'
  response = f.readline()
  assert response == 'eggs\n'
  response = f.readline()
  assert response == 'and\n'
  response = f.readline()
  assert response == 'ham\n'
  f.close()
  
  f = client.File()
  f.open(smallfile, OpenFlags.DELETE)
  f.write('green\neggs\nand\nham')
  f.readline()
  f.readline()
  f.readline()
  response = f.readline()
  assert response == 'ham'
  f.close()
  
def test_readlines():
  f = client.File()
  f.open(smallfile, OpenFlags.DELETE)
  f.write('gre\0en\neggs\nand\nham\n')
  response = f.readlines()
  assert len(response) == 4
  f.close()
  
  f = client.File()
  f.open(bigfile, OpenFlags.READ)
  size = f.stat()[1].size
   
  response = f.readlines()
  assert len(response) == 2583484
  print '+++++', len(response[-1])
  assert len(response[-1]) == 626237
  
  total = 0
  for l in f.readlines():
    #print '+++++ %r' % l
    total += len(l)
  assert total == size
  f.close()
  
def test_readchunks():
  f = client.File()
  f.open(bigfile, OpenFlags.READ)
  size = f.stat()[1].size
  
  total = 0
  for status, chunk in f.readchunks(blocksize=1024*1024*2):
    total += len(chunk)
    
  assert total == size
  f.close()

def test_vector_read():
  v = [(0, 100), (101, 200), (201, 200)]

  f = client.File()
  pytest.raises(ValueError, 'f.vector_read(v)')

  status, response = f.open(smallfile, OpenFlags.READ)
  assert status.ok
  status, response = f.vector_read(chunks=v)
  assert status.ok == False
  assert not response
  f.close()

  f = client.File()
  status, response = f.open(bigfile, OpenFlags.READ)
  print status
  assert status.ok
  status, response = f.vector_read(chunks=v)
  assert status.ok
  assert response
  f.close()

def test_stat():
  f = client.File()

  pytest.raises(ValueError, 'f.stat()')

  status, response = f.open(smallfile)
  assert status.ok

  status, response = f.stat()
  assert status.ok
  assert response.size
  f.close()

def test_sync():
  f = client.File()

  pytest.raises(ValueError, 'f.sync()')

  status, response = f.open(smallfile)
  assert status.ok

  status, response = f.sync()
  assert status.ok
  f.close()

def test_truncate():
  f = client.File()

  pytest.raises(ValueError, 'f.truncate(10000)')

  status, response = f.open(smallfile, OpenFlags.UPDATE)
  assert status.ok

  status, response = f.truncate(size=10000)
  print status
  assert status.ok
  f.close()

def test_misc():
  f = client.File()
  assert not f.is_open()

  status, response = f.open(smallfile, OpenFlags.READ)
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
