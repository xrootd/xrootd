"""
Make a directory
----------------
"""
from XRootD import client
from XRootD.client.flags import MkDirFlags

myclient = client.FileSystem("root://localhost")
myclient.mkdir("/tmp/some/dir", MkDirFlags.MAKEPATH)
