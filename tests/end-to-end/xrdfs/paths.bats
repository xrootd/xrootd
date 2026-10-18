#!/usr/bin/env bash

# Path checks of the client. A non-interactive xrdfs has no current
# directory, thus it refuses relative paths before it sends a request. The
# tests share one server.

load ../setup/server.bash

setup_file() {
    start_server server server
}

teardown_file() {
    stop_servers
}

@test "mkdir -p without a path fails" {
    run ! xrdfs "$SERVER_URL" mkdir -p
    assert_output --partial 'A path is required.'
}

@test "mkdir of a relative path fails" {
    run ! xrdfs "$SERVER_URL" mkdir foo
    assert_output --partial "Creating relative path 'foo' is disallowed."
}

@test "mkdir of a path to the parent directory fails" {
    run ! xrdfs "$SERVER_URL" mkdir ../foo
    assert_output --partial "Creating relative path '../foo' is disallowed."
}

@test "rmdir of a relative path fails" {
    run ! xrdfs "$SERVER_URL" rmdir foo
    assert_output --partial "Removing relative path 'foo' is disallowed."
}

@test "rm of a relative path fails" {
    run ! xrdfs "$SERVER_URL" rm foo
    assert_output --partial "Removing relative path 'foo' is disallowed."
}

@test "ls of a relative path fails" {
    run ! xrdfs "$SERVER_URL" ls foo
    assert_output --partial "Listing relative path 'foo' is disallowed."
}

@test "interactive mkdir of a path above the root fails" {
    run bats_pipe printf 'mkdir ../foo\nexit\n' \| xrdfs "$SERVER_URL"
    assert_output --partial "Path '../foo' escapes above root."
}

@test "mkdir -p of an absolute path creates the directory" {
    xrdfs "$SERVER_URL" mkdir -p "/t${BATS_TEST_NUMBER}/a/b"
    run -0 test -d "$SERVER_DATA/t${BATS_TEST_NUMBER}/a/b"
}

@test "rmdir of an absolute path removes the directory" {
    mkdir "$SERVER_DATA/t${BATS_TEST_NUMBER}"
    xrdfs "$SERVER_URL" rmdir "/t${BATS_TEST_NUMBER}"
    run ! test -e "$SERVER_DATA/t${BATS_TEST_NUMBER}"
}
