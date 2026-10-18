#!/usr/bin/env bash

# Every BATS file loads this file. The tests run with the bundled BATS in
# vendor/bats, thus they can use all of its features and the bats-support and
# bats-assert libraries, without a version check or a library load of their own.

bats_require_minimum_version 1.13.0
bats_load_library bats-support
bats_load_library bats-assert

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

# Stop the process and wait until it exits, so that the next server can bind
# the same port. A process which ignores SIGTERM for 5 s gets SIGKILL.
stop_pid() {
    local pid=$1

    [[ "$pid" =~ ^[0-9]+$ ]] && is_running "$pid" || return 0

    kill "$pid" 2>/dev/null

    for _ in {1..100}; do
        is_running "$pid" || return 0
        sleep 0.05
    done

    kill -9 "$pid" 2>/dev/null

    for _ in {1..100}; do
        is_running "$pid" || return 0
        sleep 0.05
    done

    return 1
}

# Stop every daemon started by the test and wait until it exits, so that the
# next test can bind the same ports.
kill_pid_files() {
    local pid

    # xrootd writes its pid both to the file given with -s and to all.pidpath,
    # without a newline, thus awk rather than cat
    for pid in $(find "$BATS_TEST_TMPDIR" -name '*.pid' -type f -exec awk '{ print }' {} + | sort -u); do
        stop_pid "$pid"
    done

    find "$BATS_TEST_TMPDIR" -name '*.pid' -type f -delete

    return 0
}

# Skip the test (or all tests of the file, in setup_file) when a command
# which it needs is not available.
require_commands() {
    local cmd

    for cmd in "$@"; do
        command -v "$cmd" >/dev/null || skip "command not found: $cmd"
    done
}

# Skip the test (or all tests of the file, in setup_file) when the file system
# of the directory has no user extended attributes.
require_xattr() {
    local dir=$1 probe status=0

    probe=$(mktemp "$dir/xattr.XXXXXX") || return 1
    if command -v setfattr > /dev/null; then
        setfattr -n user.probe -v 1 "$probe" 2>/dev/null || status=1
    elif command -v xattr > /dev/null; then
        # macOS
        xattr -w user.probe 1 "$probe" 2>/dev/null || status=1
    else
        status=1
    fi
    rm -f "$probe"

    (( status == 0 )) || skip "no extended attributes in $dir"
}

# Skip the test when the client cannot use IPv6. The client uses IPv6 only
# when an interface has an address which is not loopback and not link-local
# (see XrdNetIF::GetIF), and the server needs the loopback address ::1.
require_ipv6() {
    if [[ -r /proc/net/if_inet6 ]]; then
        grep -q '^0*1 ' /proc/net/if_inet6 &&
            awk '$6 != "lo" && $1 !~ /^fe80/ { found = 1 } END { exit !found }' \
                /proc/net/if_inet6
    else
        ifconfig 2>/dev/null | grep -q 'inet6 ::1 ' &&
            ifconfig 2>/dev/null | grep 'inet6 ' | grep -v -q -e 'inet6 fe80:' -e 'inet6 ::1 '
    fi || skip 'the host has no IPv6 address for the client'
}

# Write a file with the given number of random bytes.
make_random_file() {
    local path=$1 size=$2

    openssl rand -out "$path" "$size"
}

# Print the checksum of a local file, in the format of xrdfs query checksum.
checksum() {
    local type=$1 path=$2

    case "$type" in
        adler32) xrdadler32 < "$path" ;;
        crc32c)  xrdcrc32c < "$path" ;;
        *) echo "checksum: unknown type '$type'" >&2; return 1 ;;
    esac | cut -d' ' -f1
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
