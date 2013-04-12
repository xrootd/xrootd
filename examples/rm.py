"""
Delete a file
-------------
"""
from XRootD import client

myclient = client.FileSystem("root://localhost")
myclient.rm("/tmp/eggs")