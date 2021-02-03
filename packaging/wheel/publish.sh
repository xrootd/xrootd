#!/bin/bash

./genversion.sh
rm -r dist
python setup.py sdist
