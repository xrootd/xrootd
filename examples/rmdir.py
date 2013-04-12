"""
Delete a directory
------------------
"""
from XRootD import client

myclient = client.FileSystem("root://localhost")
myclient.rmdir("/tmp/some/dir")