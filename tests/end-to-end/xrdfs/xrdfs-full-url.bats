#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

setup() {
    launch_xrootd xrdfs-full-url.cfg xrdfs-full-url
    sleep 0.5

    run bats_pipe -0 echo 'full URL test' \| xrdcp - \
        root://localhost:11965//examplefile
}

teardown() {
    kill_pid_files 2>/dev/null || true
}

bats::on_failure() {
    print_log_files
}

@test "legacy server-first syntax remains supported" {
    run xrdfs root://localhost:11965 stat //examplefile
    assert_success
}

@test "server-first and command-first syntax cannot be mixed" {
    run xrdfs root://localhost:11965 stat \
        root://localhost:11965//examplefile
    assert_failure 1
    assert_output --partial 'cannot mix a leading endpoint with full URL operands'
}

@test "command-first syntax accepts a full URL" {
    run xrdfs stat root://localhost:11965//examplefile
    assert_success
}

@test "subcommand options are not consumed as global options" {
    run xrdfs ls -l root://localhost:11965//
    assert_success
    assert_output --partial examplefile
}

@test "URL parameters are preserved in the operand path" {
    run xrdfs stat 'root://localhost:11965//examplefile?xrdcl.test=1'
    assert_success
}

@test "URL-like xattr values remain command operands" {
    run xrdfs xattr root://localhost:11965//examplefile set \
        link=https://example.org/resource
    assert_success

    run xrdfs xattr root://localhost:11965//examplefile get link
    assert_success
    assert_output --partial 'link="https://example.org/resource"'
}

@test "multiple URLs on the same endpoint are normalized" {
    run xrdfs mv root://localhost:11965//examplefile \
        root://localhost:11965//renamed
    assert_success

    run xrdfs stat root://localhost:11965//renamed
    assert_success
}

@test "mixed URL endpoints are rejected before execution" {
    run xrdfs mv root://localhost:11965//examplefile \
        root://127.0.0.1:11965//renamed
    assert_failure 1
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "mixed URL credentials are rejected before execution" {
    run xrdfs mv root://alice@localhost:11965//examplefile \
        root://bob@localhost:11965//renamed
    assert_failure 1
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "local URLs are rejected" {
    run xrdfs stat file:///tmp/examplefile
    assert_failure 1
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "server-first query parameters may contain URL-like strings" {
    run xrdfs root://localhost:11965 query opaque \
        root://localhost:11965//examplefile
    refute_output --partial 'cannot mix a leading endpoint'
}

@test "command-first raw queries remain unsupported" {
    run xrdfs query root://localhost:11965//examplefile
    assert_failure 1
    assert_output "xrdfs: command-first full URLs are not supported for 'query'"
}

