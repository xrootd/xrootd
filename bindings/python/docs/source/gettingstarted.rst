===================
**Getting Started**
===================

``File`` and ``FileSystem`` usage
=================================

Synchronous and Asynchronous requests
-------------------------------------

The new XRootD client is capable of making both synchronous and asynchronous
requests. Therefore, ``pyxrootd`` must also be capable of this, although most
people will probably only need synchronous functionality most of the time.

Each method in the `File` and `FileSystem` classes can take an optional
``callback`` argument. If you don't pass in a callback, you're asking for a
synchronous request. If you do, the request becomes asynchronous (assuming the
callback is valid, of course), and your callback will be invoked when the
response is received.

``pyxrootd`` comes with a callback helper class:
:mod:`XRootD.client.utils.AsyncResponseHandler`. If you use an instance of this
class as your callback, you can call the :func:`wait` function whenever you
like after the request is made, and it will block until the response is
received.

Return types
------------

.. note:: The return signature of the `File` and `FileSystem` functions changes
          depending on whether you make a synchronous or asynchronous request,
          so be careful.

Synchronous requests
********************

You always get a **2-tuple** in return when you make a synchronous request. The
first item in the tuple is always an :mod:`XRootD.client.responses.XRootDStatus`
instance. The ``XRootDStatus`` object tells you whether the request was
successful or not, along with some other information.

The second item in the tuple depends on which request you made. If it's a simple
request without any response information, such as
:func:`XRootD.client.FileSystem.ping`, the second item is ``None``. Otherwise,
you get one of the objects in :mod:`XRootD.client.responses`. For example, if
you call :func:`XRootD.client.FileSystem.dirlist`, you get an instance of
:mod:`XRootD.client.responses.DirectoryList`.

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

Timeouts
--------

All of the functions in this class accept an optional ``timeout`` keyword
argument. The default timeout is `0`, which means that the environment default
will be used. You can change the timeout value on a per-request basis with the
optional parameter, or you can set it system-wide with the
``XRD_REQUESTTIMEOUT`` environment variable. Also, the timeout resolution
(time interval between timeout detection) can be set with the
``XRD_TIMEOUTRESOLUTION`` environment variable.

Copying files
=============

If you want to copy files simply and quickly with default options, you can just
use :func:`XRootD.client.FileSystem.copy`. 

But if you want more configurable copy jobs, or you want to copy a large number
of files at once, you can use :mod:`XRootD.client.CopyProcess`.

You can even pass in a copy progress handler to :func:`CopyProcess.run()` and 
use it to build some kind of progress display (much like the ``xrdcopy``
command does).

 
