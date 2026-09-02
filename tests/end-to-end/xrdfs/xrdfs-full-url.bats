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
    run bats_pipe -0 echo 'dash-prefixed file' \| xrdcp - \
        root://localhost:11965//-b
}

teardown() {
    kill_pid_files 2>/dev/null || true
}

bats::on_failure() {
    print_log_files
}

@test "legacy server-first syntax remains supported" {
    run -0 xrdfs root://localhost:11965 stat //examplefile
}

@test "server-first and command-first syntax cannot be mixed" {
    run -1 xrdfs root://localhost:11965 stat \
        root://localhost:11965//examplefile
    assert_output --partial 'cannot mix a leading endpoint with full URL operands'
}

@test "command-first syntax accepts a full URL" {
    run -0 xrdfs stat root://localhost:11965//examplefile
}

@test "cat accepts byte aliases" {
    run -0 xrdfs cat -b root://localhost:11965//examplefile
    assert_output 'full URL test'

    run -0 xrdfs cat --bytes root://localhost:11965//examplefile
    assert_output 'full URL test'
}

@test "cat option delimiter preserves dash-prefixed paths" {
    run -0 xrdfs cat -- root://localhost:11965//-b
    assert_output 'dash-prefixed file'
}

@test "subcommand options are not consumed as global options" {
    run -0 xrdfs ls -l root://localhost:11965//
    assert_output --partial examplefile
}

@test "URL parameters are preserved in the operand path" {
    run -0 xrdfs stat 'root://localhost:11965//examplefile?xrdcl.test=1'
}

@test "URL-like xattr values remain command operands" {
    run -0 xrdfs xattr root://localhost:11965//examplefile set \
        link=https://example.org/resource

    run -0 xrdfs xattr root://localhost:11965//examplefile get link
    assert_output --partial 'link="https://example.org/resource"'
}

@test "multiple URLs on the same endpoint are normalized" {
    run -0 xrdfs mv root://localhost:11965//examplefile \
        root://localhost:11965//renamed

    run -0 xrdfs stat root://localhost:11965//renamed
}

@test "mixed URL endpoints are rejected before execution" {
    run -1 xrdfs mv root://localhost:11965//examplefile \
        root://127.0.0.1:11965//renamed
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "mixed URL credentials are rejected before execution" {
    run -1 xrdfs mv root://alice@localhost:11965//examplefile \
        root://bob@localhost:11965//renamed
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "local URLs are rejected" {
    run -1 xrdfs stat file:///tmp/examplefile
    assert_output 'xrdfs: invalid remote URL operand'

    run -1 xrdfs stat stdio:///examplefile
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "invalid remote URLs are rejected" {
    run -1 xrdfs stat root://:11965//examplefile
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "server-first query parameters may contain URL-like strings" {
    run xrdfs root://localhost:11965 query opaque \
        root://localhost:11965//examplefile
    refute_output --partial 'cannot mix a leading endpoint'
}

@test "command-first raw queries remain unsupported" {
    run -1 xrdfs query root://localhost:11965//examplefile
    assert_output "xrdfs: command-first full URLs are not supported for 'query'"
}

@test "debug levels follow xrdcp conventions" {
    run -0 xrdfs -d 2 stat root://localhost:11965//examplefile
    assert_output --partial 'Network Stack:'

    run -1 xrdfs --debug 4 stat root://localhost:11965//examplefile
    assert_output "xrdfs: invalid debug level '4' (expected 0-3)"
}

@test "IPv4 network stack can be selected" {
    run -0 xrdfs -d 2 -4 stat root://localhost:11965//examplefile
    assert_output --partial 'Network Stack: IPv4'
}

@test "IPv6 network stack can be selected" {
    run -0 xrdfs -d 2 -6 stat root://localhost:11965//examplefile
    assert_output --partial 'Network Stack: IPv6'
}

@test "network stack selections are mutually exclusive" {
    run -1 xrdfs --ipv4 --ipv6 stat root://localhost:11965//examplefile
    assert_output 'xrdfs: -4 and -6 are mutually exclusive'
}
