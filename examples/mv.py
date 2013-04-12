"""
Move/rename a file
------------------
"""
from XRootD import client

myclient = client.FileSystem("root://localhost")
myclient.mv("/tmp/spam", "/tmp/eggs")