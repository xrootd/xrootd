Requirements
------------

- Python 2.4 or later
- New XRootD client 

Getting the source
------------------

::

      $ git clone git://github.com/jussy/pyxrootd.git

Installation
------------

::

      $ python setup.py install --user
      $ sudo python setup.py install

Post-installation
-----------------

if --user, in bashrc::

      $ export PATH=${HOME}/.local/bin${PATH:+:$PATH}

Browse example usages in `examples/` subdirectory 

Running the tests
-----------------

In source directory::

    $ py.test
    