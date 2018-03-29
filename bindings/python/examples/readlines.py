"""
Read all lines from a file into a list
--------------------------------------

Produces the following output (Note how the first line is not returned with the
call to :func:`readlines()` because we ate it with the first call to
:func:`readline()`)::

  'green\n'
  ['eggs\n', 'and\n', 'spam\n']

"""
from XRootD import client

with client.File() as f:
  f.open('root://localhost//tmp/eggs')

  print '%r' % f.readline()
  print f.readlines()
