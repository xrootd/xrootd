#!/bin/bash

startdir="$(pwd)"
mkdir xrootdbuild
cd xrootdbuild

if [ -z "$1" ]; then
    cmake ../.
else
    cmake ../. -DCMAKE_INSTALL_PREFIX=$1
fi

res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

cd bindings/python
make
res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

make install
res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi


python setup.py install
res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

cd $startdir
rm -r xrootdbuild
