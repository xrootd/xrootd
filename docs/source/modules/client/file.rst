==========================================
:mod:`XRootD.client.File`: File operations
==========================================

The file object is cool

.. note::

  To provide an interface like the python built-in file object, we have the 
  readline() and readlines() methods. These look for newlines in files, which
  may not always be appropriate. Also, readlines() reads the whole file, which
  is probably not a good idea.

  Also, these methods can't be given a callback, and they don't return a status
  dictionary like the others, only the data that was read.

Examples of cool usage
-----------------------

You can iterate over lines in this file, like this::

  >>> f = client.File()
  >>> f.open('root://someserver//somefile')
  >>> f.write('green\neggs\nand\nham\n')

  >>> for line in f:
  >>>   print '%r' % line
  'green\n'
  'eggs\n'
  'and\n'
  'ham\n'

Or also iterate over chunks of a particular size::

  >>> f = client.File()
  >>> f.open('root://someserver//somefile')
  >>> f.write('green\neggs\nand\nham\n')

  >>> for status, chunk in f.readchunks(offset=0, blocksize=10):
    >>> print '%r' % chunk
  'green\neggs'
  '\nand\nham'

You can also use the `with` statement on this file, like this::

  with client.File() as f:
    f.open('root://someserver//somefile')

    f.read()

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
