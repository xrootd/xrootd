from XRootD import client
from XRootD.client.utils import AsyncResponseHandler
from XRootD.client.enums import OpenFlags
from env import *

import pytest
import sys

smallfile = SERVER_URL + '/tmp/spam'
smallbuffer = 'gre\0en\neggs\nand\nham\n'
bigfile = SERVER_URL + '/tmp/bigfile'

def test_open_close_sync():
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

def test_open_close_async():
  f = client.File()
  handler = AsyncResponseHandler()
  status = f.open(smallfile, OpenFlags.READ, callback=handler)
  assert status.ok
  status, response, hostlist = handler.wait()
  assert status.ok
  assert f.is_open()
  
  handler = AsyncResponseHandler()
  status = f.close(callback=handler)
  assert status.ok
  status, response, hostlist = handler.wait()
  assert status.ok
  assert f.is_open() == False
  
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

def test_iter_small():
  f = client.File()
  f.open(smallfile, OpenFlags.DELETE)
  f.write(smallbuffer)
  size = f.stat(force=True)[1].size
  
  total = 0
  for line in f:
    print '+++++ %r' % line
    total += len(line)

  assert total == size
  f.close()

def test_iter_big():
  f = client.File()
  f.open(bigfile, OpenFlags.READ)
    
  size = f.stat()[1].size
  total = 0
    
  for line in f:
    #print '+++++ %r' % line
    total += len(line)
     
  assert total == size
  f.close()
  
def test_readline():
  f = client.File()
  f.open(smallfile, OpenFlags.DELETE)
  f.write(smallbuffer)
  
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
  f.write(smallbuffer[:-1])
  f.readline()
  f.readline()
  f.readline()
  response = f.readline()
  assert response == 'ham'
  f.close()
  
def test_readlines_small():
  f = client.File()
  f.open(smallfile, OpenFlags.DELETE)
  f.write(smallbuffer)
  response = f.readlines()
  assert len(response) == 4
  f.close()

def test_readlines_big():
  f = client.File()
  f.open(bigfile, OpenFlags.READ)
  size = f.stat()[1].size
   
#   response = f.readlines()
#   assert len(response) == len(open('/tmp/bigfile').readlines())

  #pylines = open('/tmp/bigfile').readlines()
  #nlines = len(pylines)
  
  total = 0
  lines = f.readlines()
  for i, l in enumerate(lines):
    total += len(l)
#     if l != pylines[i]:
#       print '!!!!!', i
#       print '+++++ py: %r' % pylines[i]
#       print '+++++ me: %r' % l
    
  assert total == size
  f.close()
  
def test_readchunks():
  f = client.File()
  f.open(bigfile, OpenFlags.READ)
  size = f.stat()[1].size
  
  total = 0
  for chunk in f.readchunks(blocksize=1024*1024*2):
    total += len(chunk)
    
  assert total == size
  f.close()

def test_vector_read_sync():
  v = [(0, 100), (101, 200), (201, 200)]
  vlen = sum([vec[1] for vec in v])

  f = client.File()
  pytest.raises(ValueError, 'f.vector_read(v)')
  status, response = f.open(bigfile, OpenFlags.READ)
  assert status.ok
  
  pytest.raises(TypeError, 'f.vector_read(chunks=100)')
  pytest.raises(TypeError, 'f.vector_read(chunks=[1,2,3])')
  pytest.raises(TypeError, 'f.vector_read(chunks=("lol"))')
  pytest.raises(TypeError, 'f.vector_read(chunks=[(1), (2)])')
  pytest.raises(TypeError, 'f.vector_read(chunks=[(1, 2), (3)])')
  
  status, response = f.vector_read(chunks=[(-1, -100), (-100, 10*10*10)])
  assert status.ok == False
  assert not response
  f.close()

  f = client.File()
  status, response = f.open(bigfile, OpenFlags.READ)
  assert status.ok
  status, response = f.vector_read(chunks=v)
  assert status.ok
  assert response.size == vlen
  f.close()
  
def test_vector_read_async():
  v = [(0, 100), (101, 200), (201, 200)]
  vlen = sum([vec[1] for vec in v])
  f = client.File()
  status, response = f.open(bigfile, OpenFlags.READ)
  assert status.ok
  
  handler = AsyncResponseHandler()
  status = f.vector_read(chunks=v, callback=handler)
  assert status.ok
  status, response, hostlist = handler.wait()
  print response
  print status
  assert response.size == vlen
  f.close()

def test_stat_sync():
  f = client.File()
  pytest.raises(ValueError, 'f.stat()')
  status, response = f.open(bigfile)
  
  assert status.ok
  status, response = f.stat()
  assert status.ok
  assert response.size
  f.close()
  
def test_stat_async():
  f = client.File()
  status, response = f.open(bigfile)
  assert status.ok
  
  handler = AsyncResponseHandler()
  status = f.stat(callback=handler)
  assert status.ok
  status, response, hostlist = handler.wait()
  assert status.ok
  assert response.size
  f.close()

def test_sync_sync():
  f = client.File()
  pytest.raises(ValueError, 'f.sync()')
  status, response = f.open(bigfile)
  assert status.ok
  
  status, response = f.sync()
  assert status.ok
  f.close()
  
def test_sync_async():
  f = client.File()
  status, response = f.open(bigfile)
  assert status.ok
  
  handler = AsyncResponseHandler()
  status = f.sync(callback=handler)
  status, response, hostlist = handler.wait()
  assert status.ok
  f.close()
  
def test_truncate_sync():
  f = client.File()
  pytest.raises(ValueError, 'f.truncate(10000)')
  status, response = f.open(smallfile, OpenFlags.DELETE)
  assert status.ok
  
  status, response = f.truncate(size=10000)
  assert status.ok
  f.close()
  
def test_truncate_async():
  f = client.File()
  status, response = f.open(smallfile, OpenFlags.DELETE)
  assert status.ok

  handler = AsyncResponseHandler()
  status = f.truncate(size=10000, callback=handler)
  assert status.ok
  status, response, hostlist = handler.wait()
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
