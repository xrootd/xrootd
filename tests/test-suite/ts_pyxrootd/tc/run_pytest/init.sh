#!/bin/bash
source /etc/XrdTest/utils/functions.sh

log "Initializing test case on slave" @slavename@ "..."

cd /data

if [[ @slavename@ =~ client ]]; then
  
    py.test

else
    log "Nothing to initialize." 
fi
