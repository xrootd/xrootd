#!/usr/bin/env bash

# A manager whose only data server is full: the server asks for more free
# space than its disk has. The tests share the cluster.

load ../setup/cluster.bash

setup_file() {
    start_manager manager ""
    start_data_server server manager full.cfg
    wait_for_cluster manager 1
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    echo 'no space' > local.txt
}

@test "an upload through a manager whose data server is full fails" {
    run ! xrdcp local.txt "$MANAGER_URL//t${BATS_TEST_NUMBER}"
}

# The selection of a node for a new file must not draw from an empty set of
# nodes (fixed in XrdCms)
@test "the cmsd of the manager keeps running after an upload when its data server is full" {
    xrdcp local.txt "$MANAGER_URL//t${BATS_TEST_NUMBER}" || true
    run -0 is_running "$(<"$BATS_FILE_TMPDIR/manager/cmsd.pid")"
}

@test "a read through a manager whose data server is full succeeds" {
    cp local.txt "$SERVER_DATA/t${BATS_TEST_NUMBER}"
    xrdcp "$MANAGER_URL//t${BATS_TEST_NUMBER}" copy.txt
    run -0 cmp local.txt copy.txt
}
