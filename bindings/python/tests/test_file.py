from XRootD import client
from XRootD.client.utils import AsyncResponseHandler
from XRootD.client.flags import OpenFlags, AccessMode
from env import *

import pytest
import sys
import os

# Global open mode
open_mode = (AccessMode.UR | AccessMode.UW |
             AccessMode.GR | AccessMode.GW |
             AccessMode.OR | AccessMode.OW)

def test_write_sync():
  f = client.File()
  pytest.raises(ValueError, "f.write(smallbuffer)")
  status, __ = f.open(smallfile, OpenFlags.DELETE, open_mode )
  assert status.ok

  # Write
  status, __ = f.write(smallbuffer)
  assert status.ok

  # Read
  status, response = f.read()
  assert status.ok
  assert len(response) == len(smallbuffer)

  buffer = 'eggs and ham\n'
  status, __ = f.write(buffer, offset=13, size=len(buffer) - 2)
  assert status.ok
  status, response = f.read()
  assert status.ok
  assert len(response) == len(buffer * 2) - 2
  f.close()

def test_write_async():
  f = client.File()
  status, __ = f.open(smallfile, OpenFlags.DELETE, open_mode)
  assert status.ok

  # Write async
  handler = AsyncResponseHandler()
  status = f.write(smallbuffer, callback=handler)
  status, __, __ = handler.wait()
  assert status.ok

  # Read sync
  status, response = f.read()
  assert status.ok
  assert len(response) == len(smallbuffer)
  f.close()

def test_open_close_sync():
  f = client.File()
  pytest.raises(ValueError, "f.stat()")
  status, __ = f.open(smallfile, OpenFlags.READ)
  assert status.ok
  assert f.is_open()

  # Close
  status, __ = f.close()
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
  status, __, __ = handler.wait()
  assert status.ok
  assert f.is_open()

  # Close async
  handler = AsyncResponseHandler()
  status = f.close(callback=handler)
  assert status.ok
  status, __, __ = handler.wait()
  assert status.ok
  assert f.is_open() == False

def test_io_limits():
  f = client.File()
  pytest.raises(ValueError, 'f.read()')
  status, __ = f.open(smallfile, OpenFlags.UPDATE)
  assert status.ok
  status, __ = f.stat()
  assert status.ok

  # Test read limits
  pytest.raises(TypeError, 'f.read(0, [1, 2])')
  pytest.raises(TypeError, 'f.read([1, 2], 0)')
  pytest.raises(TypeError, 'f.read(0, 10, [0, 1, 2])')
  pytest.raises(OverflowError, 'f.read(0, -10)')
  pytest.raises(OverflowError, 'f.read(-1, 1)')
  pytest.raises(OverflowError, 'f.read(0, 1, -1)')
  pytest.raises(OverflowError, 'f.read(0, 10**11)')
  pytest.raises(OverflowError, 'f.read(0, 10, 10**6)')

  # Test readline limits
  pytest.raises(TypeError, 'f.readline([0, 1], 1)')
  pytest.raises(TypeError, 'f.readline(0, [0, 1])')
  pytest.raises(TypeError, 'f.readline(0, 10, [0, 1])')
  pytest.raises(OverflowError, 'f.readline(-1, 1)')
  pytest.raises(OverflowError, 'f.readline(0, -1)')
  pytest.raises(OverflowError, 'f.readline(0, 1, -1)')
  pytest.raises(OverflowError, 'f.readline(0, 10**11)')
  pytest.raises(OverflowError, 'f.readline(0, 10, 10**11)')

  # Test write limits
  data = "data that will never get written"
  pytest.raises(TypeError, 'f.write(data, 0, [1, 2])')
  pytest.raises(TypeError, 'f.write(data, [1, 2], 0)')
  pytest.raises(TypeError, 'f.write(data, 0, 10, [0, 1, 2])')
  pytest.raises(OverflowError, 'f.write(data, 0, -10)')
  pytest.raises(OverflowError, 'f.write(data, -1, 1)')
  pytest.raises(OverflowError, 'f.write(data, 0, 1, -1)')
  pytest.raises(OverflowError, 'f.write(data, 0, 10**11)')
  pytest.raises(OverflowError, 'f.write(data, 0, 10, 10**6)')

  # Test vector_read limits
  pytest.raises(TypeError, 'f.vector_read(chunks=100)')
  pytest.raises(TypeError, 'f.vector_read(chunks=[1,2,3])')
  pytest.raises(TypeError, 'f.vector_read(chunks=[("lol", "cakes")])')
  pytest.raises(TypeError, 'f.vector_read(chunks=[(1), (2)])')
  pytest.raises(TypeError, 'f.vector_read(chunks=[(1, 2), (3)])')
  pytest.raises(OverflowError, 'f.vector_read(chunks=[(-1, -100), (-100, -100)])')
  pytest.raises(OverflowError, 'f.vector_read(chunks=[(0, 10**10*10)])')

  # Test truncate limits
  pytest.raises(TypeError, 'f.truncate(0, [1, 2])')
  pytest.raises(TypeError, 'f.truncate([1, 2], 0)')
  pytest.raises(OverflowError, 'f.truncate(-1)')
  pytest.raises(OverflowError, 'f.truncate(100, -10)')
  pytest.raises(OverflowError, 'f.truncate(0, 10**6)')
  status, __ = f.close()
  assert status.ok

def test_write_big_async():
  f = client.File()
  pytest.raises(ValueError, 'f.read()')
  status, __ = f.open(bigfile, OpenFlags.DELETE, open_mode)
  assert status.ok

  rand_data = os.urandom(64 * 1024)
  max_size = 512 * 1024  # 512 K
  offset = 0
  lst_handlers = []

  while offset <= max_size:
    status, __ = f.write(smallbuffer)
    assert status.ok
    handler = AsyncResponseHandler()
    lst_handlers.append(handler)
    status = f.write(rand_data, offset, callback=handler)
    assert status.ok
    offset = offset + len(smallbuffer) + len(rand_data)

  # Wait for async write responses
  for handler in lst_handlers:
    status, __, __ = handler.wait()
    assert status.ok

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
  status, __ = f.open(smallfile, OpenFlags.DELETE)
  assert status.ok
  status, __ = f.write(smallbuffer)
  assert status.ok

  size = f.stat(force=True)[1].size
  pylines = open('/tmp/spam').readlines()
  total = 0

  for i, line in enumerate(f):
    total += len(line)
    if pylines[i].endswith('\n'):
      assert line.endswith('\n')

  assert total == size
  f.close()

def test_iter_big():
  f = client.File()
  status, __ = f.open(bigfile, OpenFlags.READ)
  assert status.ok

  size = f.stat()[1].size
  pylines = open('/tmp/bigfile').readlines()
  total = 0

  for i, line in enumerate(f):
    total += len(line)
    if pylines[i].endswith('\n'):
      assert line.endswith('\n')

  assert total == size
  f.close()

def test_readline():
  f = client.File()
  f.open(smallfile, OpenFlags.DELETE, open_mode)
  f.write(smallbuffer)

  response = f.readline(offset=0, size=100)
  assert response == 'gre\0en\n'
  response = f.readline()
  assert response == 'eggs\n'
  response = f.readline()
  assert response == 'and\n'
  response = f.readline()
  assert response == 'ham\n'
  f.close()

  f = client.File()
  f.open(smallfile, OpenFlags.DELETE, open_mode)
  f.write(smallbuffer[:-1])
  f.readline()
  f.readline()
  f.readline()
  response = f.readline()
  assert response == 'ham'
  f.close()

def test_readlines_small():
  f = client.File()
  f.open(smallfile, OpenFlags.DELETE, open_mode)
  f.write(smallbuffer)
  f.close()
  pylines = open('/tmp/spam').readlines()

  for i in range(1, 100):
    f = client.File()
    f.open(smallfile)
    response = f.readlines(offset=0, chunksize=i)
    assert len(response) == 4
    for j, line in enumerate(response):
      if pylines[j].endswith('\n'):
        assert line.endswith('\n')
    f.close()

def test_readlines_big():
  f = client.File()
  f.open(bigfile, OpenFlags.READ)
  size = f.stat()[1].size

  lines = f.readlines()
  pylines = open('/tmp/bigfile').readlines()
  assert len(lines) == len(pylines)

  nlines = len(pylines)

  total = 0
  for i, l in enumerate(lines):
    total += len(l)
    if l != pylines[i]:
      print '!!!!!', total, i
      print '+++++ py: %r' % pylines[i]
      print '+++++ me: %r' % l
      break
    if pylines[i].endswith('\n'):
      assert l.endswith('\n')

  assert total == size
  f.close()

def test_readchunks_small():
  f = client.File()
  f.open(smallfile, OpenFlags.READ)
  size = f.stat()[1].size

  total = 0
  chunks = ['gre', '\0en', '\neg', 'gs\n', 'and', '\nha', 'm\n']
  for i, chunk in enumerate(f.readchunks(chunksize=3)):
    assert chunk == chunks[i]
    total += len(chunk)

  assert total == size
  f.close()

def test_readchunks_big():
  f = client.File()
  f.open(bigfile, OpenFlags.READ)
  size = f.stat()[1].size

  total = 0
  for chunk in f.readchunks(chunksize=1024 * 1024 * 2):
    total += len(chunk)

  assert total == size
  f.close()

def test_vector_read_sync():
  v = [(0, 100), (101, 200), (201, 200)]
  vlen = sum([vec[1] for vec in v])

  f = client.File()
  status, __ = f.open(bigfile, OpenFlags.READ)
  assert status.ok
  status, stat_info = f.stat()
  assert status.ok
  status, response = f.vector_read(chunks=v)

  # If big enough file everything shoud be ok
  if (stat_info.size > max([off + sz for (off, sz) in v])):
    assert status.ok
    assert response.size == vlen
  else:
    # If file not big enough this should fail
    status, __ = f.vector_read(chunks=v)
    assert not status.ok

  f.close()

def test_vector_read_async():
  v = [(0, 100), (101, 200), (201, 200)]
  vlen = sum([vec[1] for vec in v])
  f = client.File()
  status, __ = f.open(bigfile, OpenFlags.READ)
  assert status.ok
  status, stat_info = f.stat()
  assert status.ok
  handler = AsyncResponseHandler()
  status = f.vector_read(chunks=v, callback=handler)
  assert status.ok

  # If big enough file everything shoud be ok
  if (stat_info.size > max([off + sz for (off, sz) in v])):
    status, response, hostlist = handler.wait()
    assert status.ok
    assert response.size == vlen
  else:
    status, __, __ = handler.wait()
    assert not status.ok

  f.close()

def test_stat_sync():
  f = client.File()
  pytest.raises(ValueError, 'f.stat()')
  status, __ = f.open(bigfile)
  assert status.ok
  status, __ = f.stat()
  assert status.ok
  f.close()

def test_stat_async():
  f = client.File()
  status, response = f.open(bigfile)
  assert status.ok

  handler = AsyncResponseHandler()
  status = f.stat(callback=handler)
  assert status.ok
  status, __, __ = handler.wait()
  assert status.ok
  f.close()

def test_sync_sync():
  f = client.File()
  pytest.raises(ValueError, 'f.sync()')
  status, __ = f.open(bigfile)
  assert status.ok
  status, __ = f.sync()
  assert status.ok
  f.close()

def test_sync_async():
  f = client.File()
  status, response = f.open(bigfile)
  assert status.ok

  handler = AsyncResponseHandler()
  status = f.sync(callback=handler)
  status, __, __ = handler.wait()
  assert status.ok
  f.close()

def test_truncate_sync():
  f = client.File()
  pytest.raises(ValueError, 'f.truncate(10000)')
  status, __ = f.open(smallfile, OpenFlags.DELETE)
  assert status.ok

  status, __ = f.truncate(size=10000)
  assert status.ok
  f.close()

def test_truncate_async():
  f = client.File()
  status, __ = f.open(smallfile, OpenFlags.DELETE)
  assert status.ok

  handler = AsyncResponseHandler()
  status = f.truncate(size=10000, callback=handler)
  assert status.ok
  status, __, __ = handler.wait()
  assert status.ok
  f.close()

def test_misc():
  f = client.File()
  assert not f.is_open()

  # Open
  status, response = f.open(smallfile, OpenFlags.READ)
  assert status.ok
  assert f.is_open()

  # Set/get file properties
  f.set_property("ReadRecovery", "true")
  f.set_property("WriteRecovery", "true")
  assert f.get_property("DataServer")

  # Testing context manager
  f.close()
  assert not f.is_open()
