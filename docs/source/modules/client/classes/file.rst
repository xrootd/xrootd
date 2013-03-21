====================
File object
====================

The file object is cool

Examples of cool usage
-----------------------

You can iterate over lines in this file, like this::

  >>> f = client.File().open('root://someserver//somefile')
  >>> f.write('green\neggs\nand\nham\n')
  
  >>> for line in f:
  >>>   print '%r' % line
  'green\n'
  'eggs\n'
  'and\n'
  'ham\n'
    
Or also iterate over chunks of a particular size::

    >>> f = client.File().open('root://someserver//somefile')
    >>> f.write('green\neggs\nand\nham\n')
    
    >>> for chunk in f.readchunks(offset=0, blocksize=10:
    >>>   print '%r' % chunk
    'green\neggs'
    '\nand\nham'

You can also use the `with` statement on this file, like this::

  with client.File() as f:
    f.open('root://someserver//somefile')
    
    f.read()
    
Method reference
----------------

.. automodule:: XRootD.client

.. autoclass:: XRootD.client.File
  
  .. automethod:: open(foo='something', [bar])
  
  .. automethod:: close(foo='something', [bar])
  
  .. automethod:: stat(foo='something', [bar])
  
  .. automethod:: read(foo='something', [bar])
  
  .. automethod:: readline(foo='something', [bar])
  
  .. automethod:: readlines(foo='something', [bar])
  
  .. automethod:: readchunks(foo='something', [bar])
  
  .. automethod:: write(foo='something', [bar])
  
  .. automethod:: sync(foo='something', [bar])
  
  .. automethod:: truncate(foo='something', [bar])
  
  .. automethod:: vector_read(foo='something', [bar])
  
  .. automethod:: is_open(foo='something', [bar])
  
  .. automethod:: enable_read_recovery(foo='something', [bar])
  
  .. automethod:: enable_write_recovery(foo='something', [bar])
  
  .. automethod:: get_data_server(foo='something', [bar])
  