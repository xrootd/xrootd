#!/bin/bash
source /etc/XrdTest/utils/functions.sh

log "Running test case on slave" @slavename@ "..."

if [[ @slavename@ =~ client ]]; then

  cd /tmp/xrootd-python-master

  PYTHON=/usr/local/bin/python
  PYVER=`$PYTHON -V`
  log "Working with $PYVER from $PYTHON ..."

  log "Running pytest ..."

  export XRD_LOGLEVEL=Dump
  $PYTHON -m py.test

fi

