"""
Write a chunk of data to a file
-------------------------------

Produces the following output::

  'green\neggs\nand\nspam\n'

"""
from XRootD import client
from XRootD.client.flags import OpenFlags

with client.File() as f:
  print f.open('root://localhost//tmp/eggs', OpenFlags.UPDATE)

  data = 'spam\n'
  f.write(data, offset=15)
  print f.read()
