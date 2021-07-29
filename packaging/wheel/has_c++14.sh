#!/bin/bash

mkdir has_c++14.tmp
cp packaging/wheel/TestCXX14.txt has_c++14.tmp/CMakeLists.txt
cd has_c++14.tmp
mkdir build
cd build
cmake3 ..
has_cxx14=$?
cd ../..
rm -rf has_c++14.tmp
exit $has_cxx14