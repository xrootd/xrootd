#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

setup() {
    launch_xrootd xrdfs-full-url.cfg xrdfs-full-url
    sleep 0.5

    run bats_pipe -0 echo 'full URL test' \| "$XRDCP" - \
        root://localhost:11965//examplefile
}

teardown() {
    kill_pid_files
}

bats::on_failure() {
    print_log_files
}

@test "legacy server-first syntax remains supported" {
    run "$XRDFS" root://localhost:11965 stat //examplefile
    assert_success
}

@test "command-first syntax accepts a full URL" {
    run "$XRDFS" stat root://localhost:11965//examplefile
    assert_success
}

@test "subcommand options are not consumed as global options" {
    run "$XRDFS" ls -l root://localhost:11965//
    assert_success
    assert_output --partial examplefile
}

@test "URL parameters are preserved in the operand path" {
    run "$XRDFS" stat 'root://localhost:11965//examplefile?xrdcl.test=1'
    assert_success
}

@test "multiple URLs on the same endpoint are normalized" {
    run "$XRDFS" mv root://localhost:11965//examplefile \
        root://localhost:11965//renamed
    assert_success

    run "$XRDFS" stat root://localhost:11965//renamed
    assert_success
}

@test "mixed URL endpoints are rejected before execution" {
    run "$XRDFS" mv root://localhost:11965//examplefile \
        root://127.0.0.1:11965//renamed
    assert_failure 1
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "local URLs are rejected" {
    run "$XRDFS" stat file:///tmp/examplefile
    assert_failure 1
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "command-first raw queries remain unsupported" {
    run "$XRDFS" query root://localhost:11965//examplefile
    assert_failure 1
    assert_output "xrdfs: command-first full URLs are not supported for 'query'"
}
