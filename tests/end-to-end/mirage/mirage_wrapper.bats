#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

setup() {
    launch_xrootd mirage_wrapper.cfg mirage

    sleep 0.5

    # workdir as test tmp dir (all files are removed after execution)
    cd $BATS_TEST_TMPDIR

    printf 'stored on disk' > local.txt
}

teardown() {
    kill_pid_files
}

bats::on_failure() {
    print_log_files
}

# --- writing: disk vs memory ------------------------------------------------

@test "file written outside the mirage path is stored on disk" {
    run -0 xrdcp -f local.txt root://localhost:6543//diskfile

    assert [ -f "mirage/diskfile" ]
    assert_equal "$(cat "mirage/diskfile")" 'stored on disk'
}

@test "file written under the mirage path is not stored on disk" {
    echo 'discarded content' > local.txt

    run -0 xrdcp -f local.txt root://localhost:6543//mirage_test/memfile

    assert [ ! -e "mirage/mirage_test/memfile" ]
}

# --- reading: disk vs memory ------------------------------------------------

@test "file read from outside the mirage path returns the exact written content" {
    run -0 xrdcp -f local.txt root://localhost:6543//diskfile

    run -0 xrdcp -f root://localhost:6543//diskfile -
    assert_output 'stored on disk'
}

@test "file read from under the mirage path is served from memory at the retained size" {
    run -0 xrdcp -f local.txt root://localhost:6543//mirage_test/memfile

    # the download succeeds and yields the retained size even though the bytes
    # were discarded and nothing was ever written to disk
    run -0 xrdcp -f root://localhost:6543//mirage_test/memfile downloaded.txt
    assert_equal "$(wc -c < downloaded.txt | tr -d ' ')" 14
    assert [ ! -e "mirage/mirage_test/memfile" ]
}

@test "file size is retained under the mirage path even though its content is discarded" {
    run -0 xrdcp -f local.txt root://localhost:6543//mirage_test/memfile

    run -0 xrdfs root://localhost:6543/ stat /mirage_test/memfile
    assert_output --partial 'Size:   14'
}

# --- reading directories: disk vs memory ------------------------------------

@test "a directory outside the mirage path can be listed" {
    run -0 xrdcp -f local.txt root://localhost:6543//diskfile

    run -0 xrdfs root://localhost:6543/ ls /
    assert_output --partial '/diskfile'
}

@test "listing a directory under the mirage path returns an error" {
    run ! xrdfs root://localhost:6543/ ls /mirage_test/
    assert_output --partial '/mirage_test/'
}
