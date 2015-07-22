"""
Copy a file
-----------

See :mod:`XRootD.client.CopyProcess` if you need multiple/more configurable
copy jobs.
"""
from XRootD import client

myclient = client.FileSystem('root://localhost')
status = myclient.copy('/tmp/spam', '/tmp/eggs', force=True)
assert status[0].ok
