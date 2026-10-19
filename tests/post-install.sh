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
: "${PYTHON:=$(command -v python3 || command -v python)}"

for PROG in ${XRDCP} ${XRDFS} ${PYTHON}; do
	if [[ ! -x ${PROG} ]]; then
		echo 1>&2 "$(basename "$0"): error: '${PROG}': command not found"
		exit 1
	fi
done

V=$(${XRDCP} --version 2>&1)
echo "Using ${XRDCP} (${V#v})"
echo "Using ${PYTHON} ($(${PYTHON} --version))"
${PYTHON} -m pip show xrootd
${PYTHON} -c 'import XRootD; print(XRootD)'
${PYTHON} -c 'import pyxrootd; print(pyxrootd)'
${PYTHON} -c 'from XRootD import client; print(client)'
${PYTHON} -c 'from XRootD import client; print(client.FileSystem("root://localhost"))'

# Native installs must remain usable without the Python package. In particular,
# pip must not replace the executables with launchers or bundle another copy of
# the native libraries under site-packages.
${PYTHON} - "${XRDCP}" "${XRDFS}" <<'PY'
import configparser
from pathlib import Path
import pyxrootd
import sys

for command in sys.argv[1:]:
    with open(command, 'rb') as stream:
        assert stream.read(2) != b'#!', command + ' is a script'

copy_alias = Path(sys.argv[1]).with_name('xrdcopy')
assert copy_alias.is_symlink(), str(copy_alias) + ' is not a symlink'
assert copy_alias.resolve() == Path(sys.argv[1]).resolve()

payload = Path(pyxrootd.__file__).parent
unexpected = [path.name for path in payload.iterdir()
              if path.name not in ('__init__.py', '__pycache__')
              and not (path.name.startswith('client.')
                       and path.suffix in ('.so', '.pyd'))]
assert not unexpected, 'Bundled native payload: ' + repr(unexpected)

for metadata in payload.parent.glob('xrootd-*.dist-info'):
    entries = metadata / 'entry_points.txt'
    parser = configparser.ConfigParser()
    parser.read(str(entries))
    assert not parser.has_section('console_scripts') or not parser.options(
        'console_scripts'), 'Native wheel defines console entry points'
PY
