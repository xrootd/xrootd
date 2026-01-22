"""
Move/rename a file
------------------
"""
from XRootD import client

myclient = client.FileSystem("root://localhost")
print myclient.mv("/tmp/spam", "/tmp/eggs")
