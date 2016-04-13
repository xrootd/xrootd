"""
Delete a file
-------------
"""
from XRootD import client

myclient = client.FileSystem("root://localhost")
print myclient.rm("/tmp/eggs")
