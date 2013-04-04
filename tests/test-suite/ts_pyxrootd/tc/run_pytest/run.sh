#!/bin/bash
source /etc/XrdTest/utils/functions.sh

log "Running test case on slave" @slavename@ "..."

cd /tmp/xrootd-python-master

if [[ @slavename@ =~ client ]]; then

  PYTHON=`which python`
  PYVER=`$PYTHON -V`
  log "Working with $PYVER from $PYTHON ..."
  
  log "Running pytest ..."
  
  export XRD_LOGLEVEL=Dump
  $PYTHON -m py.test

fi


