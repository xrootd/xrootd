#!/usr/bin/env bash

# A 100 Continue must not carry Connection: Close. That header makes the
# client drop the socket after the final response, so a later PUT in the
# same call opens a new connection. Each test has its own server.

load ../../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
}

setup() {
    start_server http.cfg

    cd "$BATS_TEST_TMPDIR"
    printf 'one\n' > first
    printf 'two\n' > second
}

teardown() {
    stop_servers
}

@test "two PUTs with Expect: 100-continue reuse one connection" {
    run -0 curl --silent --show-error --fail --http1.1 --noproxy localhost \
        --header 'Expect: 100-continue' \
        --output /dev/null \
        --write-out '%{http_code} %{num_connects}\n' \
        --upload-file first "$SERVER_HTTP/first" \
        --output /dev/null \
        --upload-file second "$SERVER_HTTP/second"
    assert_output $'201 1\n201 0'
}
