
from XRootD import client
from XRootD.client.flags import OpenFlags

with client.File() as f:
  status, response = f.open('root://localhost//tmp/eggs', OpenFlags.DELETE)
  status, response = f.fcntl( 'asdf' )
  print status, response
