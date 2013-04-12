"""
Copy a file
"""
from XRootD import client

myclient = client.FileSystem('root://localhost')
status = myclient.copy('/tmp/spam', '/tmp/eggs', force=True)
print status