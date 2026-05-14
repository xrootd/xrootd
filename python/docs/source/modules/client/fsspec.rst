``XRootD.fsspec``: fsspec integration
=====================================

The Python bindings can register the ``root``, ``xroot``, ``roots``, and
``xroots`` protocols with
`fsspec <https://filesystem-spec.readthedocs.io/>`_ when the optional
``fsspec`` dependency is installed.

Install the optional dependency with:

.. code-block:: console

   pip install "xrootd[fsspec]"

Once installed, fsspec can open XRootD URLs directly:

.. code-block:: python

   import fsspec

   with fsspec.open("root://example.org//store/data.root", "rb") as f:
       data = f.read(1024)

.. module:: XRootD.fsspec

.. autoclass:: XRootD.fsspec.XRootDFileSystem
   :members:

.. autoclass:: XRootD.fsspec.XRootDFile
   :members:
