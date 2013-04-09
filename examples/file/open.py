from XRootD import client
from XRootD.client.enums import OpenFlags

f = client.File()
status, response = f.open('root://localhost//tmp/spam', OpenFlags.READ, timeout=1)
print f.is_open()
print status
print response