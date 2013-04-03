#!/bin/bash
source /etc/XrdTest/utils/functions.sh

log "Running test case on slave" @slavename@ "..."

cd /data

if [ @slavename@ =~ client ]; then
    log "hummmmmmmmmmmmmmmmmmmmmm"
else
    log "Nothing to do this time." 
fi
