
from XRootD import client
from XRootD.client.flags import OpenFlags
from time import sleep

def callback( status, response, hostlist ):
  print "Called:", status, response, hostlist

with client.File() as f:
  status, response = f.open('root://localhost//tmp/eggs', OpenFlags.DELETE)
  status = f.visa( callback = callback )
  sleep(20)
