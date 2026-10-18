#!/usr/bin/env bash

# The preload library of XrdPosix, which makes POSIX calls of a program work
# with root:// URLs, through the helpers xrdposix-cat and xrdposix-statx. The
# tests share one server.

load ../setup/server.bash

setup_file() {
    # XrdPosix does not support musl libc yet
    ! { cc -dumpmachine 2>/dev/null | grep -q musl; } || skip 'XrdPosix does not support musl'

    start_server server server
    echo 'This is a remote text file.' > "$SERVER_DATA/remote.txt"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    echo 'This is a local text file.' > local.txt
    ln -s "$SERVER_URL//remote.txt" local-to-remote.txt
    ln -s "$SERVER_URL//does/not/exist" local-to-stale.txt
}

require_statx() {
    [[ "$(uname)" == Linux ]] || skip 'statx is only on Linux'
}

@test "the preload library resolves all its symbols" {
    run env XRDPOSIX_REPORT=1 xrdposix-cat /dev/null
    refute_output --partial 'Unable to resolve'
}

@test "cat of a local file prints it" {
    run -0 xrdposix-cat local.txt
    assert_output 'This is a local text file.'
}

@test "cat of the local path of a file of the server prints it" {
    run -0 xrdposix-cat "$SERVER_DATA/remote.txt"
    assert_output 'This is a remote text file.'
}

@test "cat of a local file which does not exist fails" {
    run ! xrdposix-cat /this/does/not/exist.txt
}

@test "cat of a root URL prints the file" {
    run -0 xrdposix-cat "$SERVER_URL//remote.txt"
    assert_output 'This is a remote text file.'
}

@test "cat of a root URL which does not exist fails" {
    run ! xrdposix-cat "$SERVER_URL//does/not/exist.txt"
}

@test "cat of a link to a root URL prints the file" {
    run -0 xrdposix-cat local-to-remote.txt
    assert_output 'This is a remote text file.'
}

@test "cat of a link to a root URL which does not exist fails" {
    run ! xrdposix-cat local-to-stale.txt
}

@test "cat of a path below a virtual mount point prints the file" {
    XROOTD_VMP="localhost:$SERVER_PORT:/xrootd/=/" run -0 xrdposix-cat /xrootd/remote.txt
    assert_output 'This is a remote text file.'
}

@test "statx of a local directory succeeds" {
    require_statx
    run -0 xrdposix-statx .
}

@test "statx of the root URL of the server succeeds" {
    require_statx
    run -0 xrdposix-statx "$SERVER_URL//"
}

@test "statx of a local file succeeds" {
    require_statx
    run -0 xrdposix-statx local.txt
}

@test "statx of a root URL of a file succeeds" {
    require_statx
    run -0 xrdposix-statx "$SERVER_URL//remote.txt"
}

@test "statx of a local file which does not exist fails" {
    require_statx
    run ! xrdposix-statx /this/does/not/exist.txt
}

@test "statx of a root URL which does not exist fails" {
    require_statx
    run ! xrdposix-statx "$SERVER_URL//this/does/not/exist.txt"
}

@test "statx of a path below a virtual mount point succeeds" {
    require_statx
    XROOTD_VMP="localhost:$SERVER_PORT:/xrootd/=/" run -0 xrdposix-statx /xrootd/remote.txt
}

@test "statx of a link to a root URL succeeds" {
    require_statx
    run -0 xrdposix-statx local-to-remote.txt
}

@test "statx of a stale link succeeds, as it does not follow it" {
    require_statx
    run -0 xrdposix-statx local-to-stale.txt
}

@test "statx -L of a stale link fails" {
    require_statx
    run ! xrdposix-statx -L local-to-stale.txt
}

@test "statx -L of a link to a root URL succeeds" {
    require_statx
    run -0 xrdposix-statx -L local-to-remote.txt
}
