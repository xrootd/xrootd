#!/bin/bash

startdir="$(pwd)"
mkdir xrootdbuild
cd xrootdbuild

CMAKE_ARGS="-DXRDCL_ONLY=TRUE -DENABLE_PYTHON=TRUE -DCMAKE_INSTALL_PREFIX=$1 -DXRD_PYTHON_REQ_VERSION=$2"

cmake .. $CMAKE_ARGS

res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

make -j8
res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

cd src
make install
res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

cd ../bindings/python
python$2 setup.py install $3
res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

cd $startdir
rm -r xrootdbuild
