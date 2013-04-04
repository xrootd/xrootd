from XRootD import client
from XRootD.enums import MkDirFlags

myclient = client.FileSystem("root://localhost")

status, response = myclient.mkdir("/tmp/some/dir", MkDirFlags.MAKEPATH)
print "Status:", status
print "Response:", response