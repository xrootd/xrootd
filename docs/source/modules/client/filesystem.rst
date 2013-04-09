======================================================
:mod:`XRootD.client.FileSystem`: Filesystem operations
======================================================

.. warning:: This page is under construction

The FileSystem object is used to perform filesystem operations such as creating
directories, changing file permissions, listing directories, etc.

Synchronous and Asynchronous requests
-------------------------------------

The new XRootD client is capable of making both synchronous and asynchronous 
requests. Therefore, ``pyxrootd`` must also be capable of this, although most
people will probably only need synchronous functionality most of the time.

Each method in the `FileSystem` class can take an optional ``callback`` 
argument. If you don't pass in a callback, you're asking for a synchronous
request. If you do, the request becomes asynchronous (assuming the callback is
valid, of course), and your callback will be invoked when the response is
received.

``pyxrootd`` comes with a callback helper class: 
:mod:`XRootD.client.utils.AsyncResponseHandler`. If you use an instance of this
class as your callback, you can call the :func:`wait` function whenever you 
like after the request is made, and it will block until the response is 
received. Just thought that might be useful for someone.

Return types
------------

.. note:: Pay attention. The return signature of these functions changes 
          depending on whether you make a synchronous or asynchronous request.

Synchronous requests
********************

You always get a **2-tuple** in return when you make a synchronous request. The
first item in the tuple is always an :mod:`XRootD.client.responses.XRootDStatus`
instance. The ``XRootDStatus`` object tells you whether the request was 
successful or not, along with some other information.

The second item in the tuple depends on which request you made. If it's a simple
request without any response information, such as :func:`ping`, the second item 
is ``None``. Otherwise, you get one of the objects in 
:mod:`XRootD.client.responses`. For example, if you call :func:`dirlist`, you 
get an instance of :mod:`XRootD.client.responses.DirectoryList`.

Asynchronous requests
*********************

You get a single object, an :mod:`XRootD.client.responses.XRootDStatus`
instance, when you fire off an asynchronous request. This can inform you about
any immediate problems in making the request, e.g. the network is not reachable
(or something). 

However, when that callback you gave us (remember him?) gets triggered - you get
not 2, but a **3-tuple**. The first, again, is an ``XRootDStatus``. The second
follows the synchronous pattern, i.e. you get your response object, or ``None``.
The third item is an :mod:`XRootD.client.responses.HostList` instance. This 
contains a list of all the hosts that were implicated while carrying out that 
request you made.

Tiemouts
--------

All of the functions in this class accept an optional ``timeout`` keyword 
argument. 

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

