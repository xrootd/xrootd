"""
Iterate over a file, delimited by newline characters
----------------------------------------------------

Produces the following output::

  'green\n'
  'eggs\n'
  'and\n'
  'spam\n'

"""
from XRootD import client

with client.File() as f:
  f.open('root://localhost//tmp/eggs')

  for line in f:
    print '%r' % line