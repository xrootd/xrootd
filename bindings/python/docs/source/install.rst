=================================
Installing XRootD Python Bindings
=================================

For general instructions on how to use ``pip`` to install Python packages, please
take a look at https://packaging.python.org/en/latest/tutorials/installing-packages/.
The installation of XRootD and its Python bindings follows for the most part the
same procedure. However, there are some important things that are specific to
XRootD, which we discuss here. Since XRootD 5.6, it is possible to use ``pip`` to
install only the Python bindings, building it against a pre-installed version of
XRootD. In this case, we recommend using the same version of XRootD for both
parts, even if the newer Python bindings should be usable with older versions of
XRootD 5.x. Suppose that XRootD is installed already into ``/usr``. Then, one can
build and install the Python bindings as shown below::

  xrootd $ cd bindings/python
  python $ python3 -m pip install --target install/ .
  Processing xrootd/bindings/python
    Installing build dependencies ... done
    Getting requirements to build wheel ... done
    Installing backend dependencies ... done
    Preparing metadata (pyproject.toml) ... done
  Building wheels for collected packages: xrootd
    Building wheel for xrootd (pyproject.toml) ... done
    Created wheel for xrootd: filename=xrootd-5.6-cp311-cp311-linux_x86_64.whl size=203460 sha256=8bbd9168...
    Stored in directory: /tmp/pip-ephem-wheel-cache-rc_kb_nx/wheels/af/1b/42/bb953908...
  Successfully built xrootd
  Installing collected packages: xrootd

The command above installs the Python bindings into the ``install/`` directory in
the current working directory. The structure is as shown below::

  install/
  |-- XRootD
  |   |-- __init__.py
  |   |-- __pycache__
  |   |   |-- __init__.cpython-311.pyc
  |   |-- client
  |       |-- __init__.py
  |       |-- __pycache__
  |       |   |-- __init__.cpython-311.pyc
  |       |   |-- _version.cpython-311.pyc
  |       |   |-- copyprocess.cpython-311.pyc
  |       |   |-- env.cpython-311.pyc
  |       |   |-- file.cpython-311.pyc
  |       |   |-- filesystem.cpython-311.pyc
  |       |   |-- finalize.cpython-311.pyc
  |       |   |-- flags.cpython-311.pyc
  |       |   |-- glob_funcs.cpython-311.pyc
  |       |   |-- responses.cpython-311.pyc
  |       |   |-- url.cpython-311.pyc
  |       |   |-- utils.cpython-311.pyc
  |       |-- _version.py
  |       |-- copyprocess.py
  |       |-- env.py
  |       |-- file.py
  |       |-- filesystem.py
  |       |-- finalize.py
  |       |-- flags.py
  |       |-- glob_funcs.py
  |       |-- responses.py
  |       |-- url.py
  |       |-- utils.py
  |-- pyxrootd
  |   |-- __init__.py
  |   |-- __pycache__
  |   |   |-- __init__.cpython-311.pyc
  |   |-- client.cpython-311-x86_64-linux-gnu.so
  |-- xrootd-5.6.dist-info
      |-- INSTALLER
      |-- METADATA
      |-- RECORD
      |-- REQUESTED
      |-- WHEEL
      |-- direct_url.json
      |-- top_level.txt

  8 directories, 36 files

If you would like to install it for your own user, then use
``pip install --user`` instead of ``--target``.

If XRootD is not already installed into the system, then you will want to
install both the client libraries and the Python bindings together using ``pip``.
This is possible by using the ``setup.py`` at the top level of the project, rather
than the one in the ``bindings/python`` subdirectory::

  xrootd $ python3 -m pip install --target install/ .
  Processing xrootd
    Installing build dependencies ... done
    Getting requirements to build wheel ... done
    Installing backend dependencies ... done
    Preparing metadata (pyproject.toml) ... done
  Building wheels for collected packages: xrootd
    Building wheel for xrootd (pyproject.toml) ... done
    Created wheel for xrootd: filename=xrootd-5.6-cp311-cp311-linux_x86_64.whl size=65315683 sha256=a2e7ff52...
    Stored in directory: /tmp/pip-ephem-wheel-cache-9g6ovy4q/wheels/47/93/fc/a23666d3...
  Successfully built xrootd
  Installing collected packages: xrootd
  Successfully installed xrootd-5.6

In this case, the structure is a bit different than before::

  xrootd $ tree install/
  install/
  |-- XRootD
  |   |-- __init__.py
  |   |-- __pycache__
  |   |   |-- __init__.cpython-311.pyc
  |   |-- client
  |       |-- __init__.py
  |       |-- __pycache__
  |       |   |-- __init__.cpython-311.pyc
  |       |   |-- _version.cpython-311.pyc
  |       |   |-- copyprocess.cpython-311.pyc
  |       |   |-- env.cpython-311.pyc
  |       |   |-- file.cpython-311.pyc
  |       |   |-- filesystem.cpython-311.pyc
  |       |   |-- finalize.cpython-311.pyc
  |       |   |-- flags.cpython-311.pyc
  |       |   |-- glob_funcs.cpython-311.pyc
  |       |   |-- responses.cpython-311.pyc
  |       |   |-- url.cpython-311.pyc
  |       |   |-- utils.cpython-311.pyc
  |       |-- _version.py
  |       |-- copyprocess.py
  |       |-- env.py
  |       |-- file.py
  |       |-- filesystem.py
  |       |-- finalize.py
  |       |-- flags.py
  |       |-- glob_funcs.py
  |       |-- responses.py
  |       |-- url.py
  |       |-- utils.py
  |-- pyxrootd
  |   |-- __init__.py
  |   |-- __pycache__
  |   |   |-- __init__.cpython-311.pyc
  |   |-- client.cpython-311-x86_64-linux-gnu.so
  |   |-- libXrdAppUtils.so
  |   |-- libXrdAppUtils.so.2
  |   |-- libXrdAppUtils.so.2.0.0
  |   |-- libXrdCl.so
  |   |-- libXrdCl.so.3
  |   |-- libXrdCl.so.3.0.0
  |   |-- libXrdClHttp-5.so
  |   |-- libXrdClProxyPlugin-5.so
  |   |-- libXrdClRecorder-5.so
  |   |-- libXrdCrypto.so
  |   |-- libXrdCrypto.so.2
  |   |-- libXrdCrypto.so.2.0.0
  |   |-- libXrdCryptoLite.so
  |   |-- libXrdCryptoLite.so.2
  |   |-- libXrdCryptoLite.so.2.0.0
  |   |-- libXrdCryptossl-5.so
  |   |-- libXrdPosix.so
  |   |-- libXrdPosix.so.3
  |   |-- libXrdPosix.so.3.0.0
  |   |-- libXrdPosixPreload.so
  |   |-- libXrdPosixPreload.so.2
  |   |-- libXrdPosixPreload.so.2.0.0
  |   |-- libXrdSec-5.so
  |   |-- libXrdSecProt-5.so
  |   |-- libXrdSecgsi-5.so
  |   |-- libXrdSecgsiAUTHZVO-5.so
  |   |-- libXrdSecgsiGMAPDN-5.so
  |   |-- libXrdSeckrb5-5.so
  |   |-- libXrdSecpwd-5.so
  |   |-- libXrdSecsss-5.so
  |   |-- libXrdSecunix-5.so
  |   |-- libXrdSecztn-5.so
  |   |-- libXrdUtils.so
  |   |-- libXrdUtils.so.3
  |   |-- libXrdUtils.so.3.0.0
  |   |-- libXrdXml.so
  |   |-- libXrdXml.so.3
  |   |-- libXrdXml.so.3.0.0
  |-- xrootd-5.6.dist-info
      |-- COPYING
      |-- COPYING.BSD
      |-- COPYING.LGPL
      |-- INSTALLER
      |-- LICENSE
      |-- METADATA
      |-- RECORD
      |-- REQUESTED
      |-- WHEEL
      |-- direct_url.json
      |-- top_level.txt

  8 directories, 78 files

As can be seen above, now all client libraries have been installed alongside the
C++ Python bindings library (``client.cpython-311-x86_64-linux-gnu.so``). When
installing via ``pip`` by simply calling ``pip install xrootd``, the package that
gets installed is in this mode which includes the libraries. However, command
line tools are not included.

Binary wheels are supported as well. They can be built using the ``wheel``
subcommand instead of ``install``::

  xrootd $ python3.12 -m pip wheel .
  Processing xrootd
    Installing build dependencies ... done
    Getting requirements to build wheel ... done
    Installing backend dependencies ... done
    Preparing metadata (pyproject.toml) ... done
  Building wheels for collected packages: xrootd
    Building wheel for xrootd (pyproject.toml) ... done
    Created wheel for xrootd: filename=xrootd-5.6-cp312-cp312-linux_x86_64.whl size=65318541 sha256=6c4ed389...
    Stored in directory: /tmp/pip-ephem-wheel-cache-etujwyx1/wheels/cf/67/3c/514b21dd...
  Successfully built xrootd

If you want to have everything installed, that is, server, client, command line
tools, etc, then it is recommended to use CMake to build the project, and use
the options ``-DENABLE_PYTHON=ON -DINSTALL_PYTHON_BINDINGS=ON`` so that CMake
takes care of calling ``pip`` to install the Python bindings compiled together
with the other components in the end. The option ``-DPIP_OPTIONS`` can be used to
pass on options to pip, but it should never be used to change the installation
prefix, as that is handled by CMake. Please see INSTALL.md_ for instructions on
how to build XRootD from source using CMake.

.. _INSTALL.md: https://github.com/xrootd/xrootd/blob/master/docs/INSTALL.md
