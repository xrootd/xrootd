#!/usr/bin/env bash

# The suspend state which the cmsd of a manager sends to its xrootd: the
# manager is suspended while no data server subscribes. The xrootd logs
# "Manager <host> suspended" or "resumed" when the state changes. Each test
# starts its own cluster, as the tests change its state. A supervisor gets
# data servers only from its manager, when the manager has more than 64.

load ../setup/cluster.bash

# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=120

setup() {
    cd "$BATS_TEST_TMPDIR"
}

teardown() {
    stop_servers
}

# logged <log> <state>: the xrootd of the log changed to the state
logged() {
    grep -qE "setStatus: Manager .* $2\$" "$1"
}

# The xrootd connects to its cmsd in a thread, which can log after the end
# of the start of the node
@test "a manager without a data server connects its xrootd as suspended" {
    start_manager man ""
    run -0 wait_for 30 grep -qF 'Connected to suspended' "$MAN_LOG"
}

@test "a manager resumes its xrootd when a data server subscribes" {
    start_manager man ""
    start_data_server srv man
    run -0 wait_for 30 logged "$MAN_LOG" resumed
}

@test "a manager suspends its xrootd when its last data server stops" {
    start_manager man ""
    start_data_server srv man
    wait_for 30 logged "$MAN_LOG" resumed
    stop_node srv
    run -0 wait_for 30 logged "$MAN_LOG" suspended
}

@test "a read through a manager succeeds after it resumes" {
    start_manager man ""
    start_data_server srv man
    echo 'content' > "$SRV_DATA/file"
    wait_for 30 logged "$MAN_LOG" resumed
    run -0 xrdcp "$MAN_URL//file" -
    assert_output 'content'
}

# The login reply of the cmsd says suspended; a change before the cmsd adds
# the new xrootd to its redirectors must still reach it
@test "an xrootd which logs in while its manager is suspended resumes later" {
    start_manager man ""
    restart_server man
    start_data_server srv man
    run -0 wait_for 30 logged "$MAN_LOG" resumed
}
