#!/usr/bin/env bash

# A client which forks: with XRD_RUNFORKHANDLER=1, a child process can use a
# File object which its parent opened. The tests share one server, with the
# file /ref.dat on it.

load ../setup/server.bash

# BATS stops a test and its processes after this time, thus a client which
# hangs fails its test
# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=60

setup_file() {
    start_server server server
    make_random_file "$SERVER_DATA/ref.dat" 3000001
}

teardown_file() {
    stop_servers
}

setup() {
    export XRD_RUNFORKHANDLER=1
}

@test "children read a file which their parent opened" {
    run -0 xrdcl-api fork "$SERVER_URL//ref.dat" 5
}

@test "children read a file which their parent opened over 4 substreams" {
    XRD_SUBSTREAMSPERCHANNEL=4 run -0 xrdcl-api fork "$SERVER_URL//ref.dat" 5
}

@test "a parent which forked exits after the TTL of its idle connection" {
    # regression test for a deadlock between the fork handler and the TTL
    XRD_DATASERVERTTL=2 run -0 xrdcl-api fork "$SERVER_URL//ref.dat" 3 5
}

@test "the idle connection of a parent which forked closes after its TTL" {
    XRD_DATASERVERTTL=2 xrdcl-api fork "$SERVER_URL//ref.dat" 3 5
    run -0 grep -F 'Stream TTL elapsed, disconnecting' "$XRD_LOGFILE"
}
