==========================================
:mod:`XRootD.client.File`: File operations
==========================================

The file object is used to perform file-based operations such as reading, 
writing, vector reading, etc.

Similarities with Python built-in `file` object
-----------------------------------------------

To provide an interface like the python built-in file object, the 
__iter__(), next(), readline() and readlines() methods have been implemented. 
These look for newlines in files, which may not always be appropriate,
especially for binary data. 

Additionally, these methods can't be called asynchronously, and they don't 
return an ``XRootDStatus`` object like the others. You only get the data that 
was read.

Say we have the following file::

  >>> with client.File() as f:
  ...   f.open('root://someserver//somefile')
  ...   f.write('green\neggs\nand\nham\n')

You can iterate over lines in the file, like this::

  >>> for line in f:
  >>>   print line
  green
  eggs
  and
  ham

Read a single line from the file::

  >>> print f.readline()
  green

Read all the lines in a file at once::

  >>> print f.readlines()
  ['eggs\n', 'and\n', 'ham\n']
  
Note how the first line is not returned here, because we ate it with the first
call to :func:`readline`.

Or also iterate over chunks of a particular size::

  >>> with client.File() as f:
  ...   f.open('root://someserver//somefile')
  ...   f.write('green\neggs\nand\nham\n')
  ...   for status, chunk in f.readchunks(offset=0, blocksize=10):
  ...     print '%r' % chunk
  'green\neggs'
  '\nand\nham\n'


Class Reference
---------------

.. module:: XRootD.client

.. autoclass:: XRootD.client.File

Methods
*******

.. automethod:: XRootD.client.File.open
.. automethod:: XRootD.client.File.close
.. automethod:: XRootD.client.File.stat
.. automethod:: XRootD.client.File.read
.. automethod:: XRootD.client.File.readline
.. automethod:: XRootD.client.File.readlines
.. automethod:: XRootD.client.File.readchunks
.. automethod:: XRootD.client.File.write
.. automethod:: XRootD.client.File.sync
.. automethod:: XRootD.client.File.truncate
.. automethod:: XRootD.client.File.vector_read
.. automethod:: XRootD.client.File.is_open
.. automethod:: XRootD.client.File.enable_read_recovery
.. automethod:: XRootD.client.File.enable_write_recovery
.. automethod:: XRootD.client.File.get_data_server
