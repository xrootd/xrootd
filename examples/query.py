"""
Ask the server for some information
-----------------------------------

Produces output similar to the following::

  oss.cgroup=public&oss.space=52844687360&oss.free=27084992512&oss.maxf=27084992512&oss.used=25759694848&oss.quota=-1
"""
from XRootD import client
from XRootD.client.flags import QueryCode

myclient = client.FileSystem("root://localhost")
status, response = myclient.query(QueryCode.SPACE, '/tmp')

print response