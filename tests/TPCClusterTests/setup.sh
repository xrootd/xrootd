#!/usr/bin/env bash

# One manager in front of two data servers.

set -x

: ${XROOTD:=$(command -v xrootd)}
: ${CMSD:=$(command -v cmsd)}

servernames=("man1" "srv1" "srv2")

createDirectories() {
    DATAFOLDER="./data"
    for i in "${servernames[@]}"; do
        mkdir -p ${DATAFOLDER}/"${i}"/data
    done
}

# Only signal live pids: a stale pidfile would fail the fixture cleanup.
stop() {
    for i in "${servernames[@]}"; do
        if [[ -d "${i}" ]]; then
            for pidfile in "${i}"/cmsd.pid "${i}"/xrootd.pid; do
                test -s "${pidfile}" || continue
                pid="$(ps -o pid= "$(cat "${pidfile}")" || true)"
                if test -n "${pid}"; then
                    kill -s TERM "${pid}"
                fi
            done
            rm -rf "${i}"
        fi
    done
}

start() {
    stop
    createDirectories

    for i in "${servernames[@]}"; do
        ${XROOTD} -b -k fifo -n "${i}" -l xrootd.log -s xrootd.pid -c "${i}".cfg
    done

    for i in "${servernames[@]}"; do
        ${CMSD} -b -k fifo -n "${i}" -l cmsd.log -s cmsd.pid -c "${i}".cfg
    done

    # The manager delays requests until its data servers have reported in.
    sleep 10
}

usage() {
    echo $0 start or stop
}

[[ $# == 0 ]] && usage && exit 0

CMD=$1
shift
[[ $(type -t "${CMD}") == "function" ]] || die "unknown command: ${CMD}"
$CMD "$@"
