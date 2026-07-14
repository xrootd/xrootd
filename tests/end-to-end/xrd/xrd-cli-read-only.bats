#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

setup() {
    mkdir -p "$BATS_TEST_TMPDIR/xrd-cli-read-only/data"
    printf 'thin frontend test\n' \
        > "$BATS_TEST_TMPDIR/xrd-cli-read-only/data/sample.txt"

    launch_xrootd xrd-cli-read-only.cfg xrd-cli-read-only
    sleep 0.5

    export TEST_FILE=root://localhost:11966//data/sample.txt
    export TEST_DIRECTORY=root://localhost:11966//data/
}

teardown() {
    kill_pid_files
}

bats::on_failure() {
    print_log_files
}

@test "stat reads metadata through delegated xrdfs" {
    run "$XRD" stat "$TEST_FILE"

    assert_success
    assert_output --partial "Size:   19"
}

@test "ls reads a directory through delegated xrdfs" {
    run "$XRD" ls -lH "$TEST_DIRECTORY"

    assert_success
    assert_output --partial "sample.txt"
}

@test "cat streams file content through delegated xrdfs" {
    run "$XRD" cat -b "$TEST_FILE"

    assert_success
    assert_output "thin frontend test"
}

@test "copy downloads from the fixture to a local file" {
    local destination="$BATS_TEST_TMPDIR/download/nested/file.txt"

    run "$XRD" copy --parent "$TEST_FILE" "$destination"

    assert_success
    run cmp "$BATS_TEST_TMPDIR/xrd-cli-read-only/data/sample.txt" "$destination"
    assert_success
}
