#!/bin/bash
python setup.py egg_info
cp xrootd.egg-info/PKG-INFO .
rm -rf xrootd.egg-info
./genversion.sh
rm -r dist
python setup.py sdist
