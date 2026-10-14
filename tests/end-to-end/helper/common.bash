#!/usr/bin/env bash

# Prefer command-line tools from the active CMake build when CTest provides it.
if [[ -n "${BINARY_DIR:-}" ]]; then
    export PATH="${BINARY_DIR}/bin:${PATH}"
fi

# Start xrootd in the background and wait until it has completed its
# initialization, so that a test never races with the server startup.
launch_xrootd() {
    local config=$1
    local name=$2
    local log="$BATS_TEST_TMPDIR/$name.log"
    local pidfile="$BATS_TEST_TMPDIR/$name.pid"

    pushd "$(pwd)" 1>/dev/null
    cd "$BATS_TEST_TMPDIR"
    BATS_TEST_DIRNAME=${BATS_TEST_DIRNAME} BATS_SUITE_TMPDIR=${BATS_SUITE_TMPDIR} NAME=$name \
        xrootd -b -c "${BATS_TEST_DIRNAME}/$config" -l "$name.log" -s "$name.pid"
    popd 1>/dev/null

    # xrootd logs one of these lines at the end of its configuration; the
    # components log similar lines before it, thus match the whole line
    for _ in {1..200}; do
        if grep -qsE '^------ xrootd .* initialization completed' "$log"; then
            return 0
        fi

        if grep -qsE '^------ xrootd .* initialization failed' "$log"; then
            break
        fi

        # the pid file exists but the daemon is gone
        if [[ -s "$pidfile" ]] && ! is_running "$(<"$pidfile")"; then
            break
        fi

        sleep 0.05
    done

    echo "xrootd '$name' failed to start" >&2
    [[ -f "$log" ]] && cat "$log" >&2
    return 1
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

# Return success if the process exists and has not exited. A daemon which
# exited stays a zombie until its parent reaps it, and in a container the
# parent of a daemon may be a PID 1 which never does, thus kill -0 alone
# would report it as running.
is_running() {
    local pid=$1 stat

    if [[ -d /proc ]]; then
        stat=$(cat "/proc/$pid/stat" 2>/dev/null) || return 1
        # the state follows the command name, which is in parentheses
        stat=${stat##*) }
        [[ ${stat%% *} != Z ]]
    else
        stat=$(ps -o stat= -p "$pid" 2>/dev/null) || return 1
        [[ -n "$stat" && $stat != Z* ]]
    fi
}

# Stop every daemon started by the test and wait until it exits, so that the
# next test can bind the same ports.
kill_pid_files() {
    local pid

    # xrootd writes its pid both to the file given with -s and to all.pidpath,
    # without a newline, thus awk rather than cat
    for pid in $(find "$BATS_TEST_TMPDIR" -name '*.pid' -type f -exec awk '{ print }' {} + | sort -u); do
        [[ "$pid" =~ ^[0-9]+$ ]] && is_running "$pid" || continue

        kill "$pid" 2>/dev/null

        for _ in {1..100}; do
            is_running "$pid" || break
            sleep 0.05
        done

        # the graceful shutdown timed out
        is_running "$pid" && kill -9 "$pid" 2>/dev/null
    done

    find "$BATS_TEST_TMPDIR" -name '*.pid' -type f -delete

    return 0
}

bats::on_failure() {
    print_log_files
}

# Ask the server at the given URL for a macaroon with full access, over TLS
# verified against the test CA. Fails if the server returns no macaroon.
request_macaroon() {
    local url=$1

    curl --fail --silent --show-error \
        --cacert "$BATS_SUITE_TMPDIR/ca.pem" \
        --cert "$BATS_SUITE_TMPDIR/client.crt" --key "$BATS_SUITE_TMPDIR/client.key" \
        -X POST -H 'Content-Type: application/macaroon-request' \
        -d '{ "caveats": [ "activity:READ_METADATA,UPDATE_METADATA,LIST,DOWNLOAD,UPLOAD,MANAGE,DELETE" ], "validity": "PT1H" }' \
        "$url" | jq -er .macaroon
}
