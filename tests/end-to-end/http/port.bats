#!/usr/bin/env bash

# XrdHttp on a port of its own, next to the root protocol on the port of the
# server. The tests share one server.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl python3
    HTTP_PORT=$(free_port)
    export HTTP_PORT
    start_server server server port.cfg
    echo content > "$SERVER_DATA/file"
}

teardown_file() {
    stop_servers
}

@test "XrdHttp serves a GET on its own port" {
    run -0 curl --silent --fail "http://localhost:$HTTP_PORT/file"
    assert_output content
}

@test "the server reports the port of the root protocol at startup" {
    assert_not_equal "$SERVER_PORT" "$HTTP_PORT"
}

@test "the root protocol stays on the port of the server" {
    run -0 xrdfs "$SERVER_URL" query config version
}
