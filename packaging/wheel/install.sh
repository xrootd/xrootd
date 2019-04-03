#!/bin/bash

startdir="$(pwd)"
mkdir xrootdbuild
cd xrootdbuild

CMAKE_ARGS="-DENABLE_PYTHON=TRUE"

if [ ! -z "$1" ]; then
  CMAKE_ARGS=$CMAKE_ARGS" -DCMAKE_INSTALL_PREFIX=$1"
fi

if [ ! -z "$2" ]; then
  CMAKE_ARGS=$CMAKE_ARGS" -DXRD_PYTHON_REQ_VERSION=$2"
fi

cmake .. $CMAKE_ARGS

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


python$2 setup.py install
res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

cd $startdir
rm -r xrootdbuild
