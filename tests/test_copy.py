from XRootD import client
from XRootD.enums import OpenFlags
from env import *

def test_copy():

  f = client.File()
  s, r = f.open(SERVER_URL + '/tmp/spam', OpenFlags.DELETE )
  assert s.ok
  assert f.is_open()
  f.write('cheese and biscuits')
  size1 = f.stat(force=True)[1].size

  c = client.CopyProcess()
  c.add_job( source=SERVER_URL + '/tmp/spam', 
             target=SERVER_URL + '/tmp/eggs', force=True )
  s = c.prepare()
  assert s.ok
  s = c.run()
  assert s.ok

  f = client.File()
  s, r = f.open(SERVER_URL + '/tmp/spam', OpenFlags.READ)
  assert s.ok
  assert f.is_open()
  size2 = f.stat()[1].size
   
  assert size1 == size2
  
  f.close()