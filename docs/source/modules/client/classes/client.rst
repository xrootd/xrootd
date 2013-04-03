==================================================
:mod:`XRootD.client.Client`: Filesystem operations
==================================================

The client object is used to perform filesystem operations.

.. warning::
  This page is under construction

The `XRootDStatus` response object
----------------------------------

With each request, even asynchronous requests, you get one of these:

.. module:: XRootD.responses

.. autoclass:: XRootD.responses.XRootDStatus()
  :members:
    
The `HostInfo` response object
---------------------------------

With asynchronous requests, you even get one of these:

.. autoclass:: XRootD.responses.HostInfo()
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

.. autoclass:: XRootD.client.Client
   
Attributes
**********

.. autoattribute:: XRootD.client.Client.url

Methods
*******

.. automethod:: XRootD.client.Client.locate
.. automethod:: XRootD.client.Client.deeplocate
.. automethod:: XRootD.client.Client.mv
.. automethod:: XRootD.client.Client.query
.. automethod:: XRootD.client.Client.truncate
.. automethod:: XRootD.client.Client.rm
.. automethod:: XRootD.client.Client.mkdir
.. automethod:: XRootD.client.Client.rmdir
.. automethod:: XRootD.client.Client.chmod
.. automethod:: XRootD.client.Client.ping
.. automethod:: XRootD.client.Client.stat
.. automethod:: XRootD.client.Client.statvfs
.. automethod:: XRootD.client.Client.protocol
.. automethod:: XRootD.client.Client.dirlist
.. automethod:: XRootD.client.Client.sendinfo
.. automethod:: XRootD.client.Client.prepare

