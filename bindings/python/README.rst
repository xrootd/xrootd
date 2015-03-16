pyxrootd |buildstatus|
======================

.. |buildstatus| image::
  https://teamcity-dss.cern.ch:8443/app/rest/builds/buildType:(id:bt79)/statusIcon

Python bindings for XRootD

Requirements
------------

* Python 2.4 or later
    * Works on Python 2.4 -> 2.7
    * Python 3 not yet supported (coming soon)
* New `XRootD <http://xrootd.org/dload.html>`_ client + development headers
    * `xrootd-client, xrootd-client-devel` packages
    * Version 3.3.3 or above required

RPM Installation
----------------

If you just want to install via RPM, we build one incrementally using TeamCity.
You can get it `here <https://teamcity-dss.cern.ch:8443/guestLogin.html?guest=1>`_.
Once you've done that, you're done - you don't need to follow any of the rest of
the instructions here.

Getting the source
------------------

Clone the repository::

  $ git clone git://github.com/xrootd/xrootd-python.git

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

..
  Running the tests
  -----------------
  
  Testing requires the `pytest <https://pytest.org/latest/>`_ package.
  Once pyxrootd is installed, it may be tested (from inside the source directory)
  by running::
  
    $ py.test
