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
  s, __ = c.run()
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
  s, __ = c.run()
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
  s, __ = c.run()
  assert s.ok

def test_copy_noprep():
  c = client.CopyProcess()
  c.add_job( source=bigfile, target=bigcopy, force=True )
  s, __ = c.run()
  assert s.ok

class TestProgressHandler(object):
  def begin(self, id, total, source, target):
    print '+++ begin(): %d, total: %d' % (id, total)
    print '+++ source: %s' % source
    print '+++ target: %s' % target

  def end(self, jobId, status):
    print '+++ end(): jobId: %s, status: %s'  % (jobId, status)

  def update(self, jobId, processed, total):
    print '+++ update(): jobid: %s, processed: %d, total: %d' % (jobId, processed, total)

def test_copy_progress_handler():
  c = client.CopyProcess()
  c.add_job( source=bigfile, target=bigcopy, force=True )
  c.prepare()

  h = TestProgressHandler()
  c.run(handler=h)
