#!/usr/bin/env bash

set -ex

: "${XROOTD:=$(command -v xrootd)}"

servernames=("srv1" "srv2" "srv3" "srv4")
DATAFOLDER="./data"

setup() {
    echo "Setting up XRootD with ${servernames[*]}"

    # A run that was killed part way through leaves its servers running and its
    # files in the data folder. The uploads in test.sh fail on such files.
    teardown
    rm -rf "${DATAFOLDER}"

    mkdir -p "${DATAFOLDER}"
    for srv in "srv1" "srv2" "srv3"; do
        mkdir -p "${DATAFOLDER}/${srv}"
    done
    # srv4 shares the filesystem of srv1 (tpc.dfs)
    rm -rf "${DATAFOLDER}/srv4"
    ln -s srv1 "${DATAFOLDER}/srv4"

    # Start XRootD servers
    for srv in "${servernames[@]}"; do
        echo "Starting XRootD on ${srv}..."
        # srv3 has an intercepted close() to check for #2889
        PPFX=""
        if [[ "$srv" == "srv3" ]]; then
          if [ -e "../../lib/libcheckclose.so" ]; then
            PPFX="LD_PRELOAD=../../lib/libcheckclose.so"
          elif [ -e "../../lib/libcheckclose.dylib" ]; then
            PPFX="DYLD_INSERT_LIBRARIES=../../lib/libcheckclose.dylib"
          fi
        fi
        eval ${PPFX} ${XROOTD} -b -k fifo -n "${srv}" -l "${srv}"/xrootd.log -s "${srv}"/xrootd.pid -c "${srv}".cfg
    done

    sleep 2
    echo "XRootD setup complete."
}

teardown() {
    echo "Tearing down XRootD .."

    # Signal only processes that are still alive. A stale pid file from a
    # crashed server must not make the teardown fail.
    for srv in "${servernames[@]}"; do
        test -s "${srv}/xrootd.pid" || continue
        pid="$(ps -o pid= "$(cat "${srv}/xrootd.pid")" || true)"
        if [[ -n "${pid}" ]]; then
            kill -TERM ${pid} || true
            # Wait up to 5 seconds, so that a new server can bind the port.
            for _ in {1..50}; do
                kill -0 ${pid} 2>/dev/null || break
                sleep 0.1
            done
        fi
        rm -f "${srv}/xrootd.pid"
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
