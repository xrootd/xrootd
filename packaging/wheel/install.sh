#!/bin/bash

startdir="$(pwd)"
mkdir xrootdbuild
cd xrootdbuild

# build only client
# build python bindings
# set install prefix
# set the respective version of python
# replace the default BINDIR with a custom one that can be easily removed afterwards
#    (for the python bindings we don't want to install the binaries)
CMAKE_ARGS="-DPYPI_BUILD=TRUE -DXRDCL_LIB_ONLY=TRUE -DENABLE_PYTHON=TRUE -DCMAKE_INSTALL_PREFIX=$1/pyxrootd -DXRD_PYTHON_REQ_VERSION=$2 -DCMAKE_INSTALL_BINDIR=$startdir/xrootdbuild/bin"

if [ "$5" = "true" ]; then
  source /opt/rh/devtoolset-7/enable
fi

cmake_path=$4
$cmake_path .. $CMAKE_ARGS

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

# Determine if shutil.which is available for a modern Python package install
# (shutil.which was added in Python 3.3, so any version of Python 3 now will have it)
# TODO: Drop support for Python 3.3 and simplify to pip approach
${6} -c 'from shutil import which' &> /dev/null  # $6 holds the python sys.executable
shutil_which_available=$?
if [ "${shutil_which_available}" -ne "0" ]; then
    ${6} setup.py install ${3}
    res=$?
else
    ${6} -m pip install ${3} .
    res=$?
fi
unset shutil_which_available

if [ "$res" -ne "0" ]; then
    exit 1
fi

cd $startdir
rm -r xrootdbuild
