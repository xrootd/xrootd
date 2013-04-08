from XRootD import client
from XRootD.client.enums import OpenFlags
from env import *

smallfile = SERVER_URL + '/tmp/spam'
smallcopy = SERVER_URL + '/tmp/eggs'
buffer = 'cheese and biscuits\n'
bigfile = SERVER_URL + '/tmp/bigfile'
bigcopy = SERVER_URL + '/tmp/bigcopy'

def test_copy_smallfile():

  f = client.File()
  s, r = f.open(smallfile, OpenFlags.DELETE )
  assert s.ok
  f.write(buffer)
  size1 = f.stat(force=True)[1].size
  f.close()

  c = client.CopyProcess()
  c.add_job( source=smallfile, target=smallcopy, force=True )
  s = c.prepare()
  assert s.ok
  s = c.run()
  assert s.ok

  f = client.File()
  s, r = f.open(smallcopy, OpenFlags.READ)
  size2 = f.stat()[1].size

  assert size1 == size2
  f.close()
  
def test_copy_bigfile():
  f = client.File()
  s, r = f.open(bigfile)
  assert s.ok
  size1 = f.stat(force=True)[1].size
  f.close()
  
  c = client.CopyProcess()
  c.add_job( source=bigfile, target=bigcopy, force=True )
  s = c.prepare()
  assert s.ok
  s = c.run()
  assert s.ok
  
  f = client.File()
  s, r = f.open(bigcopy, OpenFlags.READ)
  size2 = f.stat()[1].size

  assert size1 == size2
  f.close()