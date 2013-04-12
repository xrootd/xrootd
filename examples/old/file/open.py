from XRootD import client
from XRootD.client.flags import OpenFlags

f = client.File()
status, response = f.open('root://localhost//tmp/spam', OpenFlags.READ)
print f.is_open()
print status
print response