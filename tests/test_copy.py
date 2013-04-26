from XRootD import client
from XRootD.client.flags import OpenFlags
from env import *

def test_copy_smallfile():

  f = client.File()
  s, r = f.open(smallfile, OpenFlags.DELETE )
  assert s.ok
  f.write(smallbuffer)
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

def test_copy_nojobs():
  c = client.CopyProcess()
  s = c.prepare()
  assert s.ok
  s = c.run()
  assert s.ok

def test_copy_noprep():
  c = client.CopyProcess()
  c.add_job( source=bigfile, target=bigcopy, force=True )
  s = c.run()
  assert s.ok
