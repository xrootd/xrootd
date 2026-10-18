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

# Skip the test (or all tests of the file, in setup_file) when a command
# which it needs is not available.
require_commands() {
    local cmd

    for cmd in "$@"; do
        command -v "$cmd" >/dev/null || skip "command not found: $cmd"
    done
}

# Print a TCP port on which nothing listens, for a daemon which cannot choose
# a port itself. Another process can take the port before the daemon binds
# it, which on a test host is not likely.
free_port() {
    python3 -c 'import socket; s = socket.socket(); s.bind(("localhost", 0)); print(s.getsockname()[1])'
}

# wait_for <seconds> <command...>
#
# Run the command until it succeeds, for at most the given time, for results
# which come later, such as monitoring data.
wait_for() {
    local end=$(( SECONDS + $1 ))
    shift

    until "$@"; do
        (( SECONDS < end )) || return 1
        sleep 0.2
    done
}

# Skip the test (or all tests of the file, in setup_file) when the build has
# not the plugin, for example libXrdHttp. Without a build directory, the test
# assumes that the installation has it.
require_plugin() {
    local name=$1 lib

    [[ -n "${BINARY_DIR:-}" ]] || return 0

    for lib in "$BINARY_DIR"/lib*/"$name"{,-*}.{so,dylib}; do
        [[ -e "$lib" ]] && return 0
    done

    skip "the build has no plugin $name"
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

# Ask the server at the given URL for a macaroon with full access, over TLS
# verified against the test CA. Fails if the server returns no macaroon.
request_macaroon() {
    local url=$1

    curl --fail --silent --show-error \
        --cacert "$CREDENTIALS/ca.pem" \
        --cert "$CREDENTIALS/client.crt" --key "$CREDENTIALS/client.key" \
        -X POST -H 'Content-Type: application/macaroon-request' \
        -d '{ "caveats": [ "activity:READ_METADATA,UPDATE_METADATA,LIST,DOWNLOAD,UPLOAD,MANAGE,DELETE" ], "validity": "PT1H" }' \
        "$url" | jq -er .macaroon
}
