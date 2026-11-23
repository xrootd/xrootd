#!/usr/bin/env bash

# The first request of an HTTP connection makes a login to the bridge to the
# xroot protocol. Then the request runs through the bridge.

load ../setup/server.bash

# a request which waits for the server must not block the run
# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=60

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    cd "$BATS_FILE_TMPDIR"

    start_server http.cfg trace-xrootd.cfg
    start_server busy http.cfg http-tpc.cfg login-sched.cfg

    printf '0123456789' > "$BUSY_DATA/file"
    printf '0123456789' > "$SERVER_DATA/agent"
}

teardown_file() {
    stop_servers
}

# After the login, the server must not wait for more data from the socket
@test "the first request gets a response when the server cannot keep the thread" {
    run -0 curl -sS --max-time 10 "$BUSY_HTTP/file"
    assert_output 0123456789
}

@test "three requests on one connection get responses when the server cannot keep the thread" {
    run -0 curl -sS --max-time 10 "$BUSY_HTTP/file" \
        "$BUSY_HTTP/file" "$BUSY_HTTP/file"
    assert_output 012345678901234567890123456789
}

# The server sends the user agent to the monitoring only if it is shorter
# than 1024 bytes. Then the request must run once.
@test "a request with a user agent of more than 1 KB opens the file once" {
    local agent

    printf -v agent '%*s' 1100 ''
    run -0 curl -sS --max-time 10 -A "${agent// /a}" "$SERVER_HTTP/agent"
    assert_output 0123456789
    run -0 server_log server
    assert_equal "$(grep -c ' open [a-zA-Z]* /agent$' <<< "$output")" 1
}

# The TPC handler answers OPTIONS without a login. The login for the GET then
# happens in the loop over the pipelined requests.
@test "a pipelined request after a request without login gets a response when the server cannot keep the thread" {
    local line responses=

    exec 5<> "/dev/tcp/localhost/$BUSY_PORT"
    printf 'OPTIONS /file HTTP/1.1\r\nHost: localhost\r\n\r\nGET /file HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n' >&5
    while IFS= read -r -t 10 line <&5 || [[ -n "$line" ]]; do
        responses+=$line$'\n'
        line=
    done
    exec 5<&-
    run -0 grep -ao 'HTTP/1\.1 [0-9]\{3\}' <<< "$responses"
    assert_output $'HTTP/1.1 200\nHTTP/1.1 200'
    assert_equal "${responses##*$'\r\n'}" $'0123456789\n'
}
