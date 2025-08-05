#!/usr/bin/env bash

set -x

: ${XROOTD:=$(command -v xrootd)}
: ${CMSD:=$(command -v cmsd)}

servernames=("metaman" "man1" "man2" "srv1" "srv2" "srv3" "srv4")

createDirectiories() {
    DATAFOLDER="./data"
    for i in "${servernames[@]}"; do
        mkdir -p ${DATAFOLDER}/"${i}"/data
    done
}

stop() {
    for i in "${servernames[@]}"; do
        if [[ -d "${i}" ]]; then
            kill -s TERM "$(cat "${i}"/cmsd.pid)"
            kill -s TERM "$(cat "${i}"/xrootd.pid)"
        fi
    done
}

start() {
    stop
    createDirectiories
    # start for each component
    for i in "${servernames[@]}"; do
        ${XROOTD} -b -k fifo -n "${i}" -l xrootd.log -s xrootd.pid -c "${i}".cfg
    done

    # start cmsd in the redirectors
    for i in "${servernames[@]}"; do
        ${CMSD} -b -k fifo -n "${i}" -l cmsd.log -s cmsd.pid -c "${i}".cfg
    done
    sleep 1
}


usage() {
    echo $0 start or stop
}

[[ $# == 0 ]] && usage && exit 0

CMD=$1
shift
[[ $(type -t "${CMD}") == "function" ]] || die "unknown command: ${CMD}"
$CMD "$@"
