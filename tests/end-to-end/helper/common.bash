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

# The tests must not use the credentials of the user by accident. Thus the
# setup removes the variables of a grid proxy, a certificate and a token, and
# a test which uses one of them sets its own. A path below /dev/null cannot
# exist, and a read of it fails with ENOTDIR, thus ztn stops its search for a
# token at XDG_RUNTIME_DIR, before /tmp/bt_u<uid>. Without X509_USER_PROXY,
# XrdTls and XrdClHttp use /tmp/x509up_u<uid> if it exists, thus a test which
# presents a client certificate also sets its own. Only the load in the
# context of the file sets these values, thus a test file can set its own
# credentials after the load, in setup_file(), in setup() or in a test.
if [[ -z "${BATS_TEST_TMPDIR:-}" ]]; then
    export XDG_RUNTIME_DIR=/dev/null
    export KRB5CCNAME=FILE:/dev/null/krb5cc
    unset X509_USER_PROXY X509_USER_CERT X509_USER_KEY BEARER_TOKEN BEARER_TOKEN_FILE
fi

# The tests must not read or change the files in the home directory of the
# user, for example the history file of the interactive xrdfs. Each test, and
# the setup of each file, has its own home directory, which BATS removes.
if [[ -n "${BATS_TEST_TMPDIR:-${BATS_FILE_TMPDIR:-}}" ]]; then
    export HOME="${BATS_TEST_TMPDIR:-$BATS_FILE_TMPDIR}"
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

# stop_pid_file <pidfile>
#
# Stop the process of the pid file as stop_pid does, then remove the file. A
# daemon of the setup has the path of its pid file on its command line. After
# a crash of the daemon, the system can give its pid to another process, thus
# a process without that path on its command line gets no signal.
stop_pid_file() {
    local pidfile=$1 pid

    if [[ -s "$pidfile" ]]; then
        pid=$(<"$pidfile")
        if pgrep -f -- "$pidfile" | grep -qx -- "$pid"; then
            stop_pid "$pid" || return 1
        fi
    fi
    rm -f "$pidfile"
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
        rm -f "$probe"
        skip 'no setfattr or xattr to set an extended attribute'
    fi
    rm -f "$probe"

    (( status == 0 )) || skip "no extended attributes in $dir"
}

# Skip the test when the client cannot use IPv6. The client uses IPv6 only
# when an interface which is up and runs has an address which is not
# loopback and not link-local (fe80::/10, see XrdNetIF::GetIF), and the server needs
# the loopback address ::1. Linux sets IFF_RUNNING when the operational
# state is "up", or "unknown" for an interface without one (dummy).
require_ipv6() {
    if [[ -r /proc/net/if_inet6 ]]; then
        grep -q '^0*1 ' /proc/net/if_inet6 &&
            awk '$6 != "lo" && $1 !~ /^fe[89ab]/ {
                     file = "/sys/class/net/" $6 "/operstate"; state = ""
                     getline state < file; close(file)
                     if (state == "up" || state == "unknown") found = 1
                 } END { exit !found }' /proc/net/if_inet6
    else
        ifconfig 2>/dev/null | grep -q 'inet6 ::1 ' &&
            ifconfig 2>/dev/null | awk '/^[^ \t]/ { running = /[<,]RUNNING[,>]/ }
                running && $1 == "inet6" && tolower($2) !~ /^fe[89ab][0-9a-f]:/ && $2 != "::1" { found = 1 }
                END { exit !found }'
    fi || skip 'the host has no IPv6 address for the client'
}

# Skip the test when the kernel has no IPv6, thus no socket of the IPv6 family
# can exist (Linux with ipv6.disable=1). Such a socket also connects to IPv4
# addresses, thus it needs no IPv6 address, unlike require_ipv6.
require_ipv6_socket() {
    python3 -c 'import socket; socket.socket(socket.AF_INET6, socket.SOCK_DGRAM).close()' 2>/dev/null ||
        skip 'the kernel has no IPv6 sockets'
}

# Write a file with the given number of random bytes.
make_random_file() {
    local path=$1 size=$2

    openssl rand -out "$path" "$size"
}

# Print the checksum of a local file, in the format of xrdfs query checksum.
checksum() {
    local type=$1 path=$2 sum

    case "$type" in
        adler32) sum=$(xrdadler32 < "$path") || return 1 ;;
        crc32c)  sum=$(xrdcrc32c < "$path") || return 1 ;;
        *) echo "checksum: unknown type '$type'" >&2; return 1 ;;
    esac
    echo "${sum%% *}"
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
