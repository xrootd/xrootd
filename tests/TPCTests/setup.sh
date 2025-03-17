#!/usr/bin/env bash

set -ex

: "${XROOTD:=$(command -v xrootd)}"

servernames=("srv1" "srv2")
DATAFOLDER="./data"

setup() {
    echo "Setting up XRootD with ${servernames[*]}"

    mkdir -p "${DATAFOLDER}"
    for srv in "${servernames[@]}"; do
        mkdir -p "${DATAFOLDER}/${srv}"
    done

    # Start XRootD servers
    for srv in "${servernames[@]}"; do
        echo "Starting XRootD on ${srv}..."
        ${XROOTD} -b -k fifo -n "${srv}" -l "${srv}"/xrootd.log -s "${srv}"/xrootd.pid -c "${srv}".cfg
    done

    sleep 2
    echo "XRootD setup complete."
}

teardown() {
    echo "Tearing down XRootD .."
    
    for srv in "${servernames[@]}"; do
        if [[ -f "${srv}/xrootd.pid" ]]; then
            kill -TERM "$(cat "${srv}"/xrootd.pid)" || true
        fi
    done

    echo "teardown complete."
}

# Ensure script is executed with "start" or "teardown"
case "$1" in
    start)
        setup
        ;;
    teardown)
        teardown
        ;;
    *)
        echo "Usage: $0 {start|teardown}"
        exit 1
        ;;
esac
