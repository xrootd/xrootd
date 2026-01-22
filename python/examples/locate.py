"""
Locate a file
-------------

Produces output similar to the following::

  <locations: [<type: 2, address: '[::127.0.0.1]:1094', accesstype: 1, is_manager: False, is_server: True>]>
"""
from XRootD import client
from XRootD.client.flags import OpenFlags

myclient = client.FileSystem("root://localhost")
status, locations = myclient.locate("/tmp", OpenFlags.REFRESH)

print locations
