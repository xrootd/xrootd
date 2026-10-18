#!/usr/bin/env bash

# XrdHttp on a port of its own, next to the root protocol on the port of the
# server. Each test has its own server.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl python3
}

# A new port for each test, as another file can take the port of the last
# test after its server stops
setup() {
    HTTP_PORT=$(free_port)
    export HTTP_PORT
    start_server port.cfg
    echo content > "$SERVER_DATA/file"
}

teardown() {
    stop_servers
}

@test "XrdHttp serves a GET on its own port" {
    run -0 curl --silent --fail "http://localhost:$HTTP_PORT/file"
    assert_output content
}

@test "the server reports the port of the root protocol at startup" {
    run -0 grep -F ":$SERVER_PORT initialization completed" "$SERVER_LOG"
}

@test "the root protocol stays on the port of the server" {
    run -0 xrdfs "$SERVER_URL" query config version
}
