#!/usr/bin/env bash

# Concurrent uploads over HTTP with libcurl, by the gtest xrdhttp-stress, with
# 1, 10 and 20 transfers at the same time. Each test has its own server.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    # the gtest reads the URL of the server from this file
    export TEST_CONFIG="$BATS_FILE_TMPDIR/test_config.sh"
}

setup() {
    start_server http.cfg

    echo "HOST=$SERVER_URL/" > "$TEST_CONFIG"
}

teardown() {
    stop_servers
}

# Run the test Upload/<n> of xrdhttp-stress. A filter which matches no test
# passes, thus check that the one test ran.
upload() {
    run -0 xrdhttp-stress --gtest_filter="*/HttpStressTest.Upload/$1"
    assert_line --partial '[  PASSED  ] 1 test.'
}

@test "one upload at a time succeeds" {
    upload 0
}

@test "10 concurrent uploads succeed" {
    upload 1
}

@test "20 concurrent uploads succeed" {
    upload 2
}
