Asyncio streams
===============

The high-level ``aio.open`` interface manages the native file handle, its
cursor and cancellation cleanup. It requires Python 3.11 or later and has no
fsspec dependency. The optional fsspec adapter uses the same stream class.

Importing ``aio``, ``asyncstream`` or ``fsspec`` on an older interpreter raises
a clear ``ImportError``. The classic bindings and callback API remain
installable and usable with Python 3.6, including AlmaLinux 8's system Python.

.. autofunction:: XRootD.client.aio.open

.. autoclass:: XRootD.client.asyncstream.AsyncRemoteFile
   :members:

Low-level callback bridge
-------------------------

These methods expose native responses and explicit offsets. Cancellation
stops waiting without aborting the native request. Callers are responsible
for keeping handles open until outstanding operations complete. Use the
high-level stream when automatic cleanup is required.

.. autofunction:: XRootD.client.aio.request

.. autoclass:: XRootD.client.aio.File
   :members:

.. autoclass:: XRootD.client.aio.FileSystem
   :members:
