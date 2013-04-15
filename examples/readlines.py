"""
Read all lines from a file into a list
--------------------------------------

Produces the following output (Note how the first line is not returned with the
call to :func:`readlines()` because we ate it with the first call to 
:func:`readline()`)::

  green
  ['eggs\n', 'and\n', 'spam\n']

"""
from XRootD import client

with client.File() as f:
  f.open('root://localhost//tmp/eggs')
  
#   l = f.readline(chunksize=3)
#   print '>>> %r' % l
  ls = f.readlines(chunksize=3)
  print '>>>', ls