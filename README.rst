Requirements
------------

- Python 2.4 or later
  - Works on Python 2.4 compiled with gcc-4.4.7 (redhat)
  - Python 3 not yet supported (coming soon)
- New XRootD client + development headers (`xrootd-cl, xrootd-cl-devel`
  packages)

Getting the source
------------------

Clone the repository::

  $ git clone git://github.com/jussy/pyxrootd.git

Installation
------------

If you have obtained a copy of `pyxrootd` yourself use the ``setup.py``
script to install.

To install in your `home directory 
<http://www.python.org/dev/peps/pep-0370/>`_::

  $ python setup.py install --user

To install system-wide (requires root privileges)::

  $ sudo python setup.py install

Post-installation
-----------------

If you installed `pyxrootd` into your home directory with the `--user` option
above, add ``${HOME}/.local/bin`` to your ``${PATH}`` if it is not there
already (put this in your .bashrc)::

  $ export PATH=${HOME}/.local/bin${PATH:+:$PATH}

Running the tests
-----------------

Testing requires the `pytest <https://pytest.org/latest/>`_ package.
Once pyxrootd is installed, it may be tested (from inside the source directory)
by running::

  $ py.test