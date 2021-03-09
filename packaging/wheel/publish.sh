#!/bin/bash

./genversion.sh
version=$(./genversion.sh --print-only)
version=${version#v}
echo $version > bindings/python/VERSION
rm -r dist
python3 setup.py sdist
