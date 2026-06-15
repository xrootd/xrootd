#!/bin/bash

# This script is meant as a basic test to be run post-installation
# to catch problems such as bad RPATHs, Python bindings not able to
# find XrdCl libraries post-installation, etc. Setting variables as
# LD_LIBRARY_PATH, PYTHONPATH, etc, may "fix" a problem caught by
# this script, but most likely the real fix would be to set correct
# relative RPATHs such that everything works without any extra steps
# on the part of the user. PYTHONPATH may be exceptionally set when
# the Python bindings are intentionally installed into a custom path.

set -e

: "${XRDCP:=$(command -v xrdcp)}"
: "${XRDFS:=$(command -v xrdfs)}"
: "${XRD:=$(command -v xrd)}"
: "${PYTHON:=$(command -v python3 || command -v python)}"

for PROG in ${XRDCP} ${XRDFS} ${XRD} ${PYTHON}; do
	if [[ ! -x ${PROG} ]]; then
		echo 1>&2 "$(basename "$0"): error: '${PROG}': command not found"
		exit 1
	fi
done

V=$(xrdcp --version 2>&1)
echo "Using ${XRDCP} (${V#v})"
echo "Using ${XRD} ($(${XRD} --version))"
echo "Using ${PYTHON} ($(${PYTHON} --version))"
"${XRD}" stat /tmp >/dev/null
${PYTHON} -m pip show xrootd
${PYTHON} -c 'import XRootD; print(XRootD)'
${PYTHON} -c 'import pyxrootd; print(pyxrootd)'
${PYTHON} -c 'from XRootD import client; print(client)'
${PYTHON} -c 'from XRootD import client; print(client.FileSystem("root://localhost"))'
