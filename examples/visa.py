
from XRootD import client
from XRootD.client.flags import OpenFlags

with client.File() as f:
  status, response = f.open('root://localhost:1100//tmp/eggs', OpenFlags.DELETE)
  status, response = f.visa()
  print status, response
