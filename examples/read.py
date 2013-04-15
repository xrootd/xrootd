"""
Read a certain amount of data from a certain offset in a file
-------------------------------------------------------------

Produces the following output::

  'green\neggs\nand\nham\n'
  'eggs'

"""
from XRootD import client
from XRootD.client.flags import OpenFlags

with client.File() as f:
  status, response = f.open('root://localhost//tmp/eggs', OpenFlags.DELETE)
  f.write('green\neggs\nand\nham\n')

  status, data = f.read() # Reads the whole file
  print '%r' % data

  status, data = f.read(offset=6, size=4) # Reads "eggs"
  print '%r' % data