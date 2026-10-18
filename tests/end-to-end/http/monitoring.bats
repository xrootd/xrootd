#!/usr/bin/env bash

# Monitoring of HTTP requests: the counters of the g-stream for each verb and
# status, and the status counters of the summary report. A collector receives
# both; the tests share it and one server.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    start_collector monitor
    start_server server server http monitoring.cfg
    mkdir "$SERVER_DATA/readonly" "$SERVER_DATA/dir"
    echo content > "$SERVER_DATA/file"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    N=$BATS_TEST_NUMBER
    echo upload > local.txt
}

teardown() {
    stop_servers
}

# Start a collector and a server of the test. The summary report counts the
# requests since the start of the server, thus only a server of the test
# counts the requests of the test alone.
start_own_server() {
    start_collector monitor
    start_server mine server http monitoring.cfg
    mkdir "$MINE_DATA/readonly" "$MINE_DATA/dir"
}

# Succeed when a record of the collector matches the extended regular
# expression
recorded() {
    grep -qE "$1" "$MONITOR_RECORDS"
}

@test "the g-stream counts a GET with the status 200" {
    curl --silent -o /dev/null "$SERVER_HTTP/file"
    run -0 wait_for 10 recorded '"HTTP_GET_200":\{"count":[1-9]'
}

@test "the g-stream counts a GET with the status 404" {
    curl --silent -o /dev/null "$SERVER_HTTP/missing$N"
    run -0 wait_for 10 recorded '"HTTP_GET_404":\{"count":[1-9]'
}

@test "the g-stream counts a PUT to a read-only export with the status 403" {
    curl --silent -o /dev/null --upload-file local.txt "$SERVER_HTTP/readonly/t$N"
    run -0 wait_for 10 recorded '"HTTP_PUT_403":\{"count":[1-9]'
}

@test "the g-stream counts a PUT without free inodes with the status 507" {
    curl --silent -o /dev/null --upload-file local.txt "$SERVER_HTTP/dir/$N/no_inode.txt"
    run -0 wait_for 10 recorded '"HTTP_PUT_507":\{"count":[1-9]'
}

@test "the g-stream counts a PUT to a directory with the status 409" {
    curl --silent -o /dev/null --upload-file local.txt "$SERVER_HTTP/dir"
    run -0 wait_for 10 recorded '"HTTP_PUT_409":\{"count":[1-9]'
}

@test "the g-stream counts a PROPFIND with the status 207" {
    curl --silent -o /dev/null -X PROPFIND -H 'Depth: 1' "$SERVER_HTTP/dir"
    run -0 wait_for 10 recorded '"HTTP_PROPFIND_207":\{"count":[1-9]'
}

@test "the summary report counts the responses with the status 403" {
    start_own_server
    curl --silent -o /dev/null --upload-file local.txt "$MINE_HTTP/readonly/t$N"
    run -0 wait_for 10 recorded '"http_403":[1-9]'
}

@test "the summary report counts the PUT requests" {
    start_own_server
    curl --silent -o /dev/null --upload-file local.txt "$MINE_HTTP/dir/t$N"
    run -0 wait_for 10 recorded '"PUT":[1-9]'
}
