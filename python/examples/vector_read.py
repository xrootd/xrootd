"""
Read scattered data chunks in one operation
-------------------------------------------

Produces the following output::

  <buffer: 'The XROOTD project aims at giving high p', length: 40, offset: 0>
  <buffer: 'erformance, scalable  fault tolerant acc', length: 40, offset: 40>
  <buffer: 'ess to data repositories of many kinds', length: 38, offset: 80>

"""
from XRootD import client
from XRootD.client.flags import OpenFlags

with client.File() as f:
  print f.open('root://localhost//tmp/eggs', OpenFlags.UPDATE)

  f.write(r'The XROOTD project aims at giving high performance, scalable '
          +' fault tolerant access to data repositories of many kinds')

  size = f.stat()[1].size
  v = [(0, 40), (40, 40), (80, size - 80)]

  status, response = f.vector_read(chunks=v)
  print status

  for chunk in response.chunks:
    print chunk
