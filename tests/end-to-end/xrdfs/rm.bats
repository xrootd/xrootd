#!/usr/bin/env bash

# The exit status of xrdfs rm with several files. Each test has its own
# server, with the files exists1 to exists3 on it.

load ../setup/server.bash

setup() {
    start_server

    echo "file 1" > "$SERVER_DATA/exists1"
    echo "file 2" > "$SERVER_DATA/exists2"
    echo "file 3" > "$SERVER_DATA/exists3"
}

teardown() {
    stop_servers
}

@test "rm of existing files succeeds" {
    run -0 xrdfs "$SERVER_URL" rm /exists1 /exists2 /exists3
}

@test "rm of existing files removes them" {
    xrdfs "$SERVER_URL" rm /exists1 /exists2 /exists3
    run -0 find "$SERVER_DATA" -name 'exists*'
    assert_output ''
}

@test "rm of missing files fails" {
    run ! xrdfs "$SERVER_URL" rm /missing1 /missing2 /missing3
    assert_output --partial '[3011]'
}

@test "rm of two existing files and one missing file fails" {
    run ! xrdfs "$SERVER_URL" rm /exists1 /exists2 /missing1
    assert_output --partial '[3011]'
}

@test "rm of one existing file and two missing files fails" {
    run ! xrdfs "$SERVER_URL" rm /exists1 /missing1 /missing2
    assert_output --partial '[3011]'
}
