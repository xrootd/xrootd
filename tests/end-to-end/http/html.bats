#!/usr/bin/env bash

# The HTML listing of a directory, which a GET of a directory returns. The
# tests share one server.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    start_server server server http server.cfg
    mkdir -p "$SERVER_DATA/dir/sub"
    echo content > "$SERVER_DATA/dir/sub/file"
}

teardown_file() {
    stop_servers
}

@test "a GET of a directory succeeds" {
    run -0 curl --silent -L -o /dev/null --write-out '%{http_code}' "$SERVER_HTTP/dir"
    assert_output 200
}

@test "the listing names a subdirectory with a trailing slash in its link and its text" {
    run -0 bats_pipe curl --silent -L "$SERVER_HTTP/dir" \| tr '"<>' '\n\n\n' \| grep -c 'sub/'
    assert_output 2
}
