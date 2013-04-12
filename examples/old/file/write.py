from XRootD import client
from XRootD.client.flags import OpenFlags

f = client.File()
status, response = f.open('root://localhost//tmp/eggs', OpenFlags.APPEND | OpenFlags.NEW)
assert f.is_open()

data = 'eggsandham'
s, d = f.write(data)

print s
print d
