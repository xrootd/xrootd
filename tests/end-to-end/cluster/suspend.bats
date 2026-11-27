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

# connected_suspended <log> [<lines>]: the xrootd of the log connected to its
# cmsd as suspended, after the first lines of the log. The xrootd logs a
# change of the state only after this connection.
connected_suspended() {
    tail -n +$(( ${2:-0} + 1 )) "$1" | grep -qF 'Connected to suspended'
}

# The xrootd connects to its cmsd in a thread, which can log after the end
# of the start of the node
@test "a manager without a data server connects its xrootd as suspended" {
    start_manager man ""
    run -0 wait_for 30 connected_suspended "$MAN_LOG"
}

@test "a manager resumes its xrootd when a data server subscribes" {
    start_manager man ""
    wait_for 30 connected_suspended "$MAN_LOG"
    start_data_server srv man
    run -0 wait_for 30 logged "$MAN_LOG" resumed
}

@test "a manager suspends its xrootd when its last data server stops" {
    start_manager man ""
    wait_for 30 connected_suspended "$MAN_LOG"
    start_data_server srv man
    wait_for 30 logged "$MAN_LOG" resumed
    stop_node srv
    run -0 wait_for 30 logged "$MAN_LOG" suspended
}

@test "a read through a manager succeeds after it resumes" {
    start_manager man ""
    wait_for 30 connected_suspended "$MAN_LOG"
    start_data_server srv man
    echo 'content' > "$SRV_DATA/file"
    wait_for 30 logged "$MAN_LOG" resumed
    run -0 xrdcp "$MAN_URL//file" -
    assert_output 'content'
}

# The login reply of the cmsd says suspended; a change before the cmsd adds
# the new xrootd to its redirectors must still reach it
@test "an xrootd which logs in while its manager is suspended resumes later" {
    local before
    start_manager man ""
    wait_for 30 connected_suspended "$MAN_LOG"
    before=$(wc -l < "$MAN_LOG")
    restart_server man
    wait_for 30 connected_suspended "$MAN_LOG" "$before"
    start_data_server srv man
    run -0 wait_for 30 logged "$MAN_LOG" resumed
}

# suspend_frontend <node>: suspend the front end of the cmsd of a node, as
# when its primary server goes away. A primary server logs in to the admin
# socket of the cmsd, and its disconnect suspends the front end. The cmsd logs
# the logout before it changes its state, thus wait for the log of the change.
suspend_frontend() {
    local rundir sock before

    rundir=$(_setup_rundir "$1") || return 1
    sock=$(admin_path "$1" "$1/.olb/olbd.nimda") || return 1
    [[ -S "$sock" ]] || { echo "no admin socket $sock" >&2; return 1; }
    before=$(wc -l < "$rundir/cmsd.log")
    python3 - "$sock" <<'PY'
import socket, sys
s = socket.socket(socket.AF_UNIX)
s.connect(sys.argv[1])
s.sendall(b"login p frontend\n")
s.close()
PY
    wait_for 10 sh -c "tail -n +$(( before + 1 )) '$rundir/cmsd.log' | grep -qF 'Status changed to suspended'"
}

# A redirector sees only the suspend of the service, not the one of the front
# end of its cmsd. Thus the xrootd resumes when the service resumes, also
# while the front end stays suspended.
@test "a manager resumes its xrootd while the front end of its cmsd is suspended" {
    require_commands python3
    start_manager man ""
    wait_for 30 connected_suspended "$MAN_LOG"
    suspend_frontend man
    start_data_server srv man
    run -0 wait_for 30 logged "$MAN_LOG" resumed
}

# The login reply says suspended also when only the front end is suspended
@test "an xrootd which logs in while only the front end of its manager is suspended resumes" {
    local before
    require_commands python3
    start_manager man ""
    wait_for 30 connected_suspended "$MAN_LOG"
    start_data_server srv man
    wait_for 30 logged "$MAN_LOG" resumed
    suspend_frontend man
    before=$(wc -l < "$MAN_LOG")
    restart_server man
    run -0 wait_for 30 connected_suspended "$MAN_LOG" "$before"
    run -0 wait_for 30 sh -c "tail -n +$(( before + 1 )) '$MAN_LOG' | grep -qE 'setStatus: Manager .* resumed\$'"
}
