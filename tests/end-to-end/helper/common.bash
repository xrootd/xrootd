#!/usr/bin/env bash

launch_xrootd() {
    local config=$1
    local name=$2

    pushd $(pwd) 1>/dev/null
    cd $BATS_TEST_TMPDIR
    BATS_TEST_DIRNAME=${BATS_TEST_DIRNAME} NAME=$name xrootd -b -c ${BATS_TEST_DIRNAME}/$config -l $name.log -s $name.pid
    popd 1>/dev/null
}

print_log_files() {
    if [[ -z "${BATS_SKIP_SERVER_LOGS}" ]]; then
        for file in $(find $BATS_TEST_TMPDIR -name '*.log' -type f ! -empty); do
            name="${file##*/}"
            name="${name%.*}"
            printf '\n'
            sed "s|^|[$name] |" "$file"
            printf '\n'
        done
    fi
}

kill_pid_files() {
    find $BATS_TEST_TMPDIR -name '*.pid' -type f ! -empty -exec awk '{ print }' {} \; | xargs -r kill -9
}