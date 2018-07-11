#!/bin/bash

startdir="$(pwd)"
mkdir xrootdbuild
cd xrootdbuild

if [ -z "$1" ]; then
    cmake ../.
else
    cmake ../. -DCMAKE_INSTALL_PREFIX=$1
fi

make
make install

cd bindings/python
python setup.py install

cd $startdir
rm -r xrootdbuild
