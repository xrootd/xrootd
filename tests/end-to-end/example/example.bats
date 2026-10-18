#!/usr/bin/env bash

# An example of an end-to-end test file, to copy for a new one. See
# setup/server.bash for the setup functions.

# The common setup loads bats-support and bats-assert, and sets the client
# defaults: a log at the Debug level for each test, short timeouts, and unix
# authentication.
load ../setup/server.bash

# setup_file runs once, before all tests of the file. Its files are in
# BATS_FILE_TMPDIR, which BATS removes at the end.
setup_file() {
    echo 'this file is visible to all tests in this file' > "$BATS_FILE_TMPDIR/file.txt"
}

# setup runs before each test. A test has its own directory BATS_TEST_TMPDIR
# and its own server, thus the tests are independent. The server exports
# EXAMPLE_URL, EXAMPLE_DATA and more. Only a file with a setup which takes
# long, such as a cache or a cluster, starts its servers in setup_file, for
# all its tests (see xrdfs/query.bats).
setup() {
    start_server example

    echo 'example file!' > "$EXAMPLE_DATA/examplefile"

    cd "$BATS_TEST_TMPDIR"
    REMOTE="//t${BATS_TEST_NUMBER}"
    echo 'this file is visible only to this test' > file_test.txt
}

# teardown runs after each test, also after a failure, and stops the servers
# of the test.
teardown() {
    stop_servers
}

@test "create file from stdin should succeed" {
    run -0 bats_pipe echo 'hello world!' \| xrdcp - "$EXAMPLE_URL/$REMOTE"
}

@test "create file from disk should succeed" {
    echo 'hello world!' > localfile
    run -0 xrdcp localfile "$EXAMPLE_URL/$REMOTE"
}

@test "download of inexistent file fails" {
    run ! xrdcp "$EXAMPLE_URL/$REMOTE" -
}

@test "download of existing file should succeed" {
    run -0 xrdcp -f "$EXAMPLE_URL//examplefile" /dev/null
}

@test "contents of a downloaded file should match" {
    run -0 xrdcp "$EXAMPLE_URL//examplefile" -
    assert_output 'example file!'
}

# A test which needs a plugin or a command checks for it first, and skips
# without it.
@test "contents of a file downloaded over http should match" {
    require_plugin libXrdHttp
    require_commands curl
    start_server web http.cfg
    echo 'example file!' > "$WEB_DATA/examplefile"
    run -0 curl -sf "$WEB_HTTP/examplefile"
    assert_output 'example file!'
}

@test "contents of file in BATS_FILE_TMPDIR file should match" {
    run -0 cat "$BATS_FILE_TMPDIR/file.txt"
    assert_output 'this file is visible to all tests in this file'
}

@test "contents of file in BATS_TEST_TMPDIR file should match" {
    run -0 cat "$BATS_TEST_TMPDIR/file_test.txt"
    assert_output 'this file is visible only to this test'
}

# A test which needs another server starts it itself; stop_servers in
# teardown stops it.
@test "a server of a test answers" {
    start_server mine
    run -0 xrdfs "$MINE_URL" query config version
}
