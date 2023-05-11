#!/bin/bash

./genversion.sh >| VERSION

[[ -d dist ]] && rm -rf dist

# Determine if wheel.bdist_wheel is available for wheel.bdist_wheel in setup.py
python3 -c 'import wheel' &> /dev/null
wheel_available=$?
if [ "${wheel_available}" -ne "0" ]; then
    python3 -m pip install wheel
    wheel_available=$?

    if [ "${wheel_available}" -ne "0" ]; then
        echo "ERROR: Unable to find wheel and unable to install wheel with '$(command -v python3) -m pip install wheel'."
        echo "       Please ensure wheel is available to $(command -v python3) and try again."
        exit 1
    fi
fi
unset wheel_available

# Determine if build is available
python3 -c 'import build' &> /dev/null
build_available=$?
if [ "${build_available}" -ne "0" ]; then
    python3 -m pip install build
    build_available=$?
fi

if [ "${build_available}" -ne "0" ]; then
    echo "WARNING: Unable to find build and unable to install build from PyPI with '$(command -v python3) -m pip install build'."
    echo "         Falling back to building sdist with '$(command -v python3) setup.py sdist'."
    python3 setup.py sdist
else
    python3 -m build --sdist .
fi
unset build_available
