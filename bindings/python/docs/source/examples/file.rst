=================
``File`` examples
=================

This page includes some simple examples of how to use the ``pyxrootd`` `File`
object to manipulate files on an ``xrootd`` server.

We'll use the following `File` object as a basis for the rest of the examples::

  from XRootD import client
  from XRootD.client.flags import OpenFlags

  with client.File() as f:
    f.open('root://someserver//tmp/eggs', OpenFlags.UPDATE)
    f.write('green\neggs\nand\nham\n')

.. -----------------------------------------------------------------------------
.. read
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/read.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/read.py
   :lines: 17-

.. include:: ../../../examples/read.py
   :start-line: 4
   :end-line: 8

.. -----------------------------------------------------------------------------
.. write
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/write.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/write.py
   :lines: 16-

.. include:: ../../../examples/write.py
   :start-line: 4
   :end-line: 8

.. -----------------------------------------------------------------------------
.. iterate
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/iterate.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/iterate.py
   :lines: 18-

.. include:: ../../../examples/iterate.py
   :start-line: 4
   :end-line: 10

.. -----------------------------------------------------------------------------
.. readlines
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/readlines.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/readlines.py
   :lines: 18-

.. include:: ../../../examples/readlines.py
   :start-line: 4
   :end-line: 10

.. -----------------------------------------------------------------------------
.. readchunks
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/readchunks.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/readchunks.py
   :lines: 16-

.. include:: ../../../examples/readchunks.py
   :start-line: 4
   :end-line: 8

.. -----------------------------------------------------------------------------
.. vector_read
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/vector_read.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/vector_read.py
   :lines: 17-

.. include:: ../../../examples/vector_read.py
   :start-line: 4
   :end-line: 9
