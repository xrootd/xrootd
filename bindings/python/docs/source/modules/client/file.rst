================================================
:mod:`XRootD.client.File`: File-based operations
================================================

.. module:: XRootD.client
   :noindex:

.. autoclass:: XRootD.client.File

Similarities with Python built-in `file` object
-----------------------------------------------

To provide an interface like the python built-in file object, the
__iter__(), next(), readline() and readlines() methods have been implemented.
These look for newlines in files, which may not always be appropriate,
especially for binary data.

Additionally, these methods can't be called asynchronously, and they don't
return an ``XRootDStatus`` object like the others. You only get the data that
was read.

Class Reference
---------------

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
.. automethod:: XRootD.client.File.set_property
.. automethod:: XRootD.client.File.get_property
