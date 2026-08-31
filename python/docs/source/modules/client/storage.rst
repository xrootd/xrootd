StorageClient
=============

.. autoclass:: XRootD.client.StorageClient
   :members:

``StorageClient`` provides thread-safe application operations with one default
deadline and object-scoped authentication.  A client created with
``from_environment`` owns and closes its authentication context::

  from XRootD import client

  with client.StorageClient.from_environment(timeout=300) as storage:
      storage.probe('davs://storage.example/rucio', timeout=10)
      info = storage.info(
          'davs://storage.example/rucio/file',
          checksum_algorithms=('adler32', 'md5'),
          require_checksum=True)

.. autoclass:: XRootD.client.StorageInfo
   :members:
