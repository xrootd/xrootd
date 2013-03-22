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
    
Method reference
----------------

.. module:: XRootD.client

.. autoclass:: XRootD.client.File
  
  .. automethod:: open(url[, flags[, mode[, timeout[, callback]]]])
  
  .. automethod:: close([timeout[, callback]])

    As of Python 2.5, you can avoid having to call this method explicitly if you
    use the :keyword:`with` statement.  For example, the following code will
    automatically close *f* when the :keyword:`with` block is exited::

      from __future__ import with_statement # This isn't required in Python 2.6

      with client.File() as f:
        f.open("root://someserver//somefile")
        for line in f:
          print line,
  
  .. automethod:: stat([force[, timeout[, callback]]])
  
  .. automethod:: read([offset[, size[, timeout[, callback]]]])
  
  .. automethod:: readline([offset[, size]])
  
  .. automethod:: readlines([offset[, size]])
  
  .. automethod:: readchunks([offset[, blocksize]])
  
  .. automethod:: write(buffer[, offset[, size[, timeout[, callback]]]])
  
  .. automethod:: sync([timeout[, callback]])
  
  .. automethod:: truncate(size[, timeout[, callback]])
  
  .. automethod:: vector_read(chunks[, timeout[, callback]])
  
    +---------------------------------------------+
    | The `vector read info` return dictionary    |
    +==============+==============================+
    | `chunks`     | List of chunks               |
    |              |                              |
    |              | +--------------------------+ |
    |              | | `chunk` sub-dictionary   | |
    |              | +==========+===============+ |
    |              | | `buffer` | le buffer     | |
    |              | +----------+---------------+ |
    |              | | `length` | le length     | |
    |              | +----------+---------------+ |
    |              | | `offset` | le offset     | |
    |              | +----------+---------------+ |
    +--------------+------------------------------+
    | `size`       | The size                     |
    +--------------+------------------------------+
  
  .. automethod:: is_open()
  
  .. automethod:: enable_read_recovery(enable)
  
  .. automethod:: enable_write_recovery(enable)
  
  .. automethod:: get_data_server()
  