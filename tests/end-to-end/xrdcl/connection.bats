#!/usr/bin/env bash

# Connection errors and the idle time to live (TTL) of connections to a data
# server. The tests share one server, and GONE_URL has the port
# NO_SERVER_PORT, on which no server runs.

load ../setup/server.bash

# BATS stops a test and its processes after this time, thus a client which
# hangs fails its test
# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=60

GONE_URL="root://localhost:$NO_SERVER_PORT"

setup_file() {
    start_server server server

    echo 'content' > "$SERVER_DATA/file"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
}

# Send two stat requests in one session of interactive xrdfs, with an idle
# time between them, longer than the data server TTL of 1 s.
stat_twice() {
    { echo 'stat /file'; sleep 3; echo 'stat /file'; } |
        XRD_DATASERVERTTL=1 xrdfs "$SERVER_URL"
}

@test "a client connects to the server by the name of the host" {
    local host
    host=$(hostname -f 2>/dev/null || uname -n)
    getent hosts "$host" > /dev/null 2>&1 || skip "the name of the host does not resolve"
    run -0 xrdfs "root://$host:$SERVER_PORT" query config version
}

@test "a server which does not run gives a connection error" {
    XRD_CONNECTIONRETRY=1 run ! xrdfs "$GONE_URL" query config version
    assert_output --partial 'Connection error'
}

@test "a request to a server which does not run expires after the request timeout" {
    XRD_REQUESTTIMEOUT=1 run ! xrdfs "$GONE_URL" query config version
    assert_output --partial 'Operation expired'
}

@test "an idle connection to a data server closes after its TTL" {
    stat_twice > /dev/null
    run -0 grep -F 'Stream TTL elapsed, disconnecting' "$XRD_LOGFILE"
}

@test "a request after the TTL of a data server reconnects" {
    run -0 stat_twice
    assert_equal "$(grep -c '^Size:' <<< "$output")" 2
}

@test "16 substreams with a short TTL do not hang the client" {
    XRD_SUBSTREAMSPERCHANNEL=16 run -0 stat_twice
}

@test "opens of 20 users with 16 substreams and a short TTL each fail with no such file" {
    # each user has 17 sockets; the default limit of macOS is 256 files
    (( $(ulimit -n) >= 1024 )) || ulimit -n 1024 || skip 'cannot open 1024 files'
    XRD_SUBSTREAMSPERCHANNEL=16 XRD_DATASERVERTTL=1 XRD_CONNECTIONRETRY=0 \
        run -0 xrdcl-api open-users "localhost:$SERVER_PORT" /missing 20 4
    assert_equal "$(grep -c '^3011$' <<< "$output")" 20
}
