"""
Delete a directory
------------------
"""
from XRootD import client

myclient = client.FileSystem("root://localhost")
print myclient.rmdir("/tmp/some/dir")
