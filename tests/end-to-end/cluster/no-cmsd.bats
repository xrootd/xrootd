#!/usr/bin/env bash

# A manager whose cmsd has stopped: its xrootd cannot locate files. The tests
# share the cluster; setup_file stops the cmsd of the manager. A data server
# without a manager has no cmsd either, but serves its files.

load ../setup/cluster.bash

setup_file() {
    start_manager manager ""
    start_data_server server manager
    wait_for_cluster manager 1
    # without the pid file, stop_servers cannot stop another process which
    # reuses the pid
    stop_pid "$(<"$BATS_FILE_TMPDIR/manager/cmsd.pid")"
    rm -f "$BATS_FILE_TMPDIR/manager/cmsd.pid"
    echo 'no cmsd' > "$SERVER_DATA/file"
    start_server lone server lone.cfg
    echo 'no cmsd' > "$LONE_DATA/file"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
}

@test "a read through a manager whose cmsd stopped fails" {
    XRD_REQUESTTIMEOUT=5 run ! xrdcp -f "$MANAGER_URL//file" copy.txt
}

@test "a manager whose cmsd stopped logs that all managers are dysfunctional" {
    XRD_REQUESTTIMEOUT=5 xrdcp -f "$MANAGER_URL//file" copy.txt || true
    run -0 grep -F 'All managers are dysfunctional' "$MANAGER_LOG"
}

@test "the xrootd of a manager whose cmsd stopped keeps running" {
    XRD_REQUESTTIMEOUT=5 xrdcp -f "$MANAGER_URL//file" copy.txt || true
    run -0 server_is_up "$MANAGER_URL"
}

@test "a data server without a manager serves its files" {
    xrdcp -f "$LONE_URL//file" copy.txt
    run -0 cmp "$LONE_DATA/file" copy.txt
}
