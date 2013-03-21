1.1. Requirements
-----------------

1.2. Getting the source
-----------------------

      `git clone git://github.com/jussy/pyxrootd.git`

1.3. Installation
-----------------

      python setup.py install --user
      sudo python setup.py install

1.4. Post-installation
----------------------

if --user, in bashrc:

      export PATH=${HOME}/.local/bin${PATH:+:$PATH}

* Browse example usages in `examples/` subdirectory 

1.4. Running the tests
----------------------

      `py.test` in src dir