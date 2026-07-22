#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

setup() {
    launch_xrootd mirage.cfg mirage

    sleep 0.5

    echo 0 | xrdcp - root://localhost:6543//remotefile

    cd $BATS_TEST_TMPDIR
}

teardown() {
    kill_pid_files
}

bats::on_failure() {
    print_log_files
}

@test "local and remote size should match" {
    local_size=$(wc -c < $BATS_TEST_DIRNAME/mirage.cfg | tr -d ' ')
    run -0 xrdcp -f $BATS_TEST_DIRNAME/mirage.cfg root://localhost:6543//mirage.cfg
    run -0 xrdcp -f root://localhost:6543//mirage.cfg downloaded.cfg
    assert_equal "$(wc -c < downloaded.cfg | tr -d ' ')" "$local_size"
}

@test "stat should return correct file size" {
    local_size=$(wc -c < $BATS_TEST_DIRNAME/mirage.cfg | tr -d ' ')
    run -0 xrdcp -f $BATS_TEST_DIRNAME/mirage.cfg root://localhost:6543//mirage.cfg
    run -0 xrdfs root://localhost:6543/ stat /mirage.cfg
    assert_output --partial "$local_size"
}

@test "truncate to 1000 bytes should succeed" {
    run -0 xrdfs root://localhost:6543/ truncate /remotefile 1000
    run -0 xrdfs root://localhost:6543/ stat /remotefile
    assert_output --partial "1000"
}

@test "single char pattern should repeat 1000 times" {
    run -0 xrdfs root://localhost:6543/ truncate /remotefile 1000
    run -0 xrdfs root://localhost:6543/ xattr /remotefile set pattern='a'
    occurrences=$(xrdcp -f root://localhost:6543//remotefile - | grep -o a | wc -l | tr -d ' ')
    assert_equal "$occurrences" "1000"
}

@test "string pattern should repeat 200 times" {
    run -0 xrdfs root://localhost:6543/ truncate /remotefile 1000
    run -0 xrdfs root://localhost:6543/ xattr /remotefile set pattern='abcde'
    occurrences=$(xrdcp -f root://localhost:6543//remotefile - | grep -o abcde | wc -l | tr -d ' ')
    assert_equal "$occurrences" "200"
}

@test "open return code should propagate error" {
    run -0 xrdfs root://localhost:6543/ xattr /remotefile set open.return_code=12
    run xrdcp -f root://localhost:6543//remotefile -
    assert_output --partial "cannot allocate memory"
}

@test "read return code should propagate error" {
    run -0 xrdfs root://localhost:6543/ xattr /remotefile set read.return_code=12
    run xrdcp -f root://localhost:6543//remotefile -
    assert_output --partial "cannot allocate memory"
}
