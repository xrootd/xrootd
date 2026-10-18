#!/usr/bin/env bash

# A redirect of a request over HTTPS to HTTP on the same server. The response
# must close the TLS connection, also for a keep-alive request. The tests share
# one server.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands python3
    start_server server server http tls redirect.cfg
}

teardown_file() {
    stop_servers
}

# The response headers of a keep-alive GET over TLS, read until the server
# closes the connection
redirect() {
    python3 "$BATS_TEST_DIRNAME/https-redirect.py" "$SERVER_PORT" "$CREDENTIALS/ca.pem" | tr -d '\r'
}

@test "a GET over HTTPS gets a redirect to HTTP" {
    run -0 redirect
    assert_line 'HTTP/1.1 302 Found'
}

@test "the redirect closes a keep-alive connection" {
    run -0 redirect
    assert_line --regexp '^[Cc]onnection: [Cc]lose$'
}
