=======================
``FileSystem`` examples
=======================

This page includes some simple examples of basic usage of the ``pyxrootd``
`FileSystem` object to interact with an ``xrootd`` server.

We'll use the following imports and `FileSystem` object as the basis for the
rest of the examples::

  from XRootD import client
  from XRootD.client.flags import DirListFlags, OpenFlags, MkDirFlags, QueryCode

  myclient = client.FileSystem('root://someserver:1094')

.. -----------------------------------------------------------------------------
.. copy
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/copy.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/copy.py
   :lines: 11-

.. include:: ../../../examples/copy.py
   :start-line: 4
   :end-line: 6

.. -----------------------------------------------------------------------------
.. dirlist
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/dirlist.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/dirlist.py
   :lines: 16-

.. include:: ../../../examples/dirlist.py
   :start-line: 4
   :end-line: 9

.. -----------------------------------------------------------------------------
.. mkdir
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/mkdir.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/mkdir.py
   :lines: 9-

.. -----------------------------------------------------------------------------
.. rmdir
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/rmdir.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/rmdir.py
   :lines: 8-

.. -----------------------------------------------------------------------------
.. mv
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/mv.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/mv.py
   :lines: 8-

.. -----------------------------------------------------------------------------
.. rm
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/rm.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/rm.py
   :lines: 8-

.. -----------------------------------------------------------------------------
.. locate
.. -----------------------------------------------------------------------------

.. include:: ../../../examples/locate.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/locate.py
   :lines: 13-

.. include:: ../../../examples/locate.py
   :start-line: 4
   :end-line: 7

.. -----------------------------------------------------------------------------
.. query
.. -----------------------------------------------------------------------------
.. include:: ../../../examples/query.py
   :start-line: 1
   :end-line: 3

.. literalinclude:: ../../../examples/query.py
   :lines: 17-

.. include:: ../../../examples/query.py
   :start-line: 4
   :end-line: 11

