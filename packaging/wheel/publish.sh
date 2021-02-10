#!/bin/bash

./genversion.sh
rm -r dist
python3 setup.py sdist
