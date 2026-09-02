=====================================================
:mod:`XRootD.client.TapeClient`: Tape REST operations
=====================================================

``TapeClient`` provides synchronous helpers for WLCG Tape REST discovery,
staging, polling, cancellation, release, deletion, and archive locality.

Site-defined Tape REST metadata can be passed explicitly::

  from XRootD.client import TapeClient

  tape = TapeClient(timeout=600)
  status, request = tape.stage(
      'https://storage.example.org/path/file',
      targeted_metadata={'example-site': {'queue': 'bulk'}})

The object is sent unchanged as the request's ``targetedMetadata``. Individual
file dictionaries can supply ``targeted_metadata``; protocol-shaped
dictionaries may use ``targetedMetadata`` directly.

Class Reference
---------------

.. module:: XRootD.client

.. autoclass:: XRootD.client.TapeClient
   :members:
