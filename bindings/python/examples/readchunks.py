"""
Iterate over a file in chunks of the specified size
---------------------------------------------------

Produces the following output::

  'green\neggs'
  '\nand\nspam\n'

"""
from XRootD import client

with client.File() as f:
  f.open('root://localhost//tmp/eggs')

  for chunk in f.readchunks(offset=0, chunksize=10):
    print '%r' % chunk
