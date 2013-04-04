======================================================
:mod:`XRootD.client.FileSystem`: Filesystem operations
======================================================

.. warning::
  This page is under construction

The FileSystem object is used to perform filesystem operations such as creating
directories, changing file permissions, listing directories, etc.

The `XRootDStatus` response object
----------------------------------

With each request, even asynchronous requests, you get one of these:

.. module:: XRootD.client.responses

.. autoclass:: XRootD.client.responses.XRootDStatus()
  :members:

The `HostInfo` response object
---------------------------------

With asynchronous requests, you even get one of these:

.. autoclass:: XRootD.client.responses.HostInfo()
  :members:

.. note::
  All methods accept keyword arguments.

.. note::
  All methods accept an optional callback argument. This will make the function
  call happen asynchronously. All methods also accept optional timeout argument.

.. note::
  **For all synchronous function calls**, you get a tuple in return. The tuple 
  contains the status dictionary and the response dictionary, if any. If the 
  function has no return dictionary, the second item of the tuple will be None.

  **For all asynchronous requests**, you will get the initial status of the 
  request when the call returns, i.e. you can discover network problems etc, 
  but not the server response. When your callback is invoked, you will get 
  another status dict and the response dict (if any, otherwise None). You also
  get the host info

Class Reference
---------------

.. module:: XRootD.client

.. autoclass:: XRootD.client.FileSystem

Attributes
**********

.. autoattribute:: XRootD.client.FileSystem.url

Methods
*******

.. automethod:: XRootD.client.FileSystem.locate
.. automethod:: XRootD.client.FileSystem.deeplocate
.. automethod:: XRootD.client.FileSystem.mv
.. automethod:: XRootD.client.FileSystem.query
.. automethod:: XRootD.client.FileSystem.truncate
.. automethod:: XRootD.client.FileSystem.rm
.. automethod:: XRootD.client.FileSystem.mkdir
.. automethod:: XRootD.client.FileSystem.rmdir
.. automethod:: XRootD.client.FileSystem.chmod
.. automethod:: XRootD.client.FileSystem.ping
.. automethod:: XRootD.client.FileSystem.stat
.. automethod:: XRootD.client.FileSystem.statvfs
.. automethod:: XRootD.client.FileSystem.protocol
.. automethod:: XRootD.client.FileSystem.dirlist
.. automethod:: XRootD.client.FileSystem.sendinfo
.. automethod:: XRootD.client.FileSystem.prepare

