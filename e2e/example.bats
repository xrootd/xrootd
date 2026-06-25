#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ./helper/common.bash

setup_file() {
    # workdir as file tmp dir (all files are removed after execution)
    cd $BATS_FILE_TMPDIR

    echo 'this file is visible to all tests in this file' > file.txt
}

setup() {
	launch_xrootd example.cfg example

	sleep 0.5

    echo 'example file!' | xrdcp - root://localhost:1094//examplefile

    # workdir as test tmp dir (all files are removed after execution)
    cd $BATS_TEST_TMPDIR

    echo 'this file is visible only to this test' > file_test.txt
}

teardown() {
    kill_pid_files
}

bats::on_failure() {
	print_log_files
}

@test "create file from stdin should succeed" {
    run bats_pipe -0 echo 'hello world!' \| xrdcp - root://localhost:1094//remotefile
}

@test "create file from disk should succeed" {
    echo 'hello world!' > localfile
    run -0 xrdcp localfile root://localhost:1094//remotefile
}

@test "download of inexistent file fails" {
    run ! xrdcp root://localhost:1094//remotefile -
}

@test "download of existing file should succeed" {
    run -0 xrdcp -f root://localhost:1094//examplefile /dev/null
}

@test "contents of a downloaded file should match" {
    run -0 xrdcp root://localhost:1094//examplefile -
    assert_output 'example file!'
}

@test "contents of file in BATS_FILE_TMPDIR file should match" {
    run -0 cat $BATS_FILE_TMPDIR/file.txt
    assert_output 'this file is visible to all tests in this file'
}

@test "contents of file in BATS_TEST_TMPDIR file should match" {
    run -0 cat $BATS_TEST_TMPDIR/file_test.txt
    assert_output 'this file is visible only to this test'
}
