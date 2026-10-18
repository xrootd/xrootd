#!/usr/bin/env bash

# Listings of large and empty directories, and moves of directories. The tests
# share one server, with 1000 empty files in /bigdir on it.

load ../setup/server.bash

setup_file() {
    start_server server server

    mkdir "$SERVER_DATA/bigdir"
    for i in $(seq 1000); do
        : > "$SERVER_DATA/bigdir/file$i"
    done
}

teardown_file() {
    stop_servers
}

setup() {
    DIR="/d${BATS_TEST_NUMBER}"
}

@test "ls of a directory with 1000 files shows 1000 entries" {
    run -0 bats_pipe xrdfs "$SERVER_URL" ls /bigdir \| wc -l
    assert_output --regexp '^ *1000$'
}

@test "ls -l of a directory with 1000 files shows 1000 entries" {
    run -0 bats_pipe xrdfs "$SERVER_URL" ls -l /bigdir \| wc -l
    assert_output --regexp '^ *1000$'
}

@test "ls of an empty directory shows nothing" {
    xrdfs "$SERVER_URL" mkdir "$DIR"
    run -0 xrdfs "$SERVER_URL" ls "$DIR"
    assert_output ''
}

@test "mv of a directory moves its files" {
    mkdir "$SERVER_DATA$DIR"
    echo 'content' > "$SERVER_DATA$DIR/file"
    xrdfs "$SERVER_URL" mv "$DIR" "$DIR.moved"
    run -0 xrdfs "$SERVER_URL" stat "$DIR.moved/file"
}

@test "mv of a directory removes the old path" {
    mkdir "$SERVER_DATA$DIR"
    xrdfs "$SERVER_URL" mv "$DIR" "$DIR.moved"
    run ! xrdfs "$SERVER_URL" stat "$DIR"
    assert_output --partial '[3011]'
}
