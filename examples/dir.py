from XRootD.client import FileSystem
from XRootD.file import File

print dir(client)

c = FileSystem('root://localhost')
print dir(c)
print c.stat('/tmp')[1].modTimeAsString

print "\n\n"
print c.url
print type(c.url)
print dir(c.url)
print c.url.hostid

