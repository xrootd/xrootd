AuthContext
===========

.. autoclass:: XRootD.client.AuthContext
   :members:

Authentication contexts attach credentials to individual XRootD client
objects without changing process-wide environment values.  A context may be
shared by concurrent operations.  Separate contexts use separate authenticated
channels, including when they target the same endpoint.

Bearer token example::

  from XRootD import client

  with client.AuthContext.bearer(token=token) as auth:
      filesystem = client.FileSystem('root://storage.example/', auth=auth)
      status, info = filesystem.stat('/data/file', timeout=30)

X.509 proxy example::

  auth = client.AuthContext.x509(proxy='/tmp/x509up_u1000')
  copy = client.CopyProcess(auth=auth)
  copy.add_job('root://storage.example//data/file', 'file:///tmp/file')
  copy.prepare()
  status, results = copy.run()

Environment-snapshot example::

  auth = client.AuthContext.from_environment(fallback='none')

This resolves standard bearer, XRootD GSI, X.509, and TLS environment values
once without mutating them.  ``fallback='none'`` prevents later ambient
credential selection when no usable credential is present.
