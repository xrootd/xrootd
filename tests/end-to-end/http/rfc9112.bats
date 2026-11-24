#!/usr/bin/env bash

# Conformance tests for RFC 9112 (HTTP/1.1). Each test checks one rule, and
# its name gives the section of the RFC.

load ../setup/server.bash

# a server which keeps a connection open must not block the run
# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=60

setup_file() {
    require_plugin libXrdHttp
    cd "$BATS_FILE_TMPDIR"

    start_server http.cfg tls.cfg

    printf '0123456789' > "$SERVER_DATA/file"
    mkdir "$SERVER_DATA/dir"
}

setup() {
    requests=
}

teardown_file() {
    stop_servers
}

# Append a request for the given method and path to $requests. The extra
# headers may use \r\n escapes.
request() {
    local method=$1 path=$2 extra=${3:-} body=${4:-} version=${5:-HTTP/1.1} r

    printf -v r '%s %s %s\r\nHost: localhost\r\n%b\r\n%s' \
        "$method" "$path" "$version" "$extra" "$body"
    requests+=$r
}

# exchange <port> [tls]
#
# Send $requests on one connection. Print the status codes which the server
# sends, then "closed" if the server closes the connection in 5 s, or "open"
# if it does not. The file $BATS_TEST_TMPDIR/responses keeps all that the
# server sent. macOS has no timeout(1), thus a loop with kill -0.
exchange() {
    local responses=$BATS_TEST_TMPDIR/responses pid state=closed i

    printf '%s' "$requests" > "$BATS_TEST_TMPDIR/requests"
    if [[ ${2:-} == tls ]]; then
        # -quiet also keeps the connection open at the end of the input
        openssl s_client -quiet -connect "localhost:$1" \
            < "$BATS_TEST_TMPDIR/requests" > "$responses" 2> /dev/null &
    else
        # A subshell keeps the descriptor out of the shell of the test
        (
            exec 5<> "/dev/tcp/localhost/$1"
            cat "$BATS_TEST_TMPDIR/requests" >&5
            exec cat <&5
        ) > "$responses" 2> /dev/null &
    fi
    pid=$!
    for ((i = 0; i < 50; i++)); do
        kill -0 "$pid" 2> /dev/null || break
        sleep 0.1
    done
    if kill -0 "$pid" 2> /dev/null; then
        state=open
        kill "$pid" 2> /dev/null || true
    fi
    wait "$pid" 2> /dev/null || true
    grep -ao 'HTTP/1\.[01] [0-9]\{3\}' "$responses" | cut -d' ' -f2 || true
    echo "$state"
}

# The Connection header of the last response
connection_header() {
    tr -d '\r' < "$BATS_TEST_TMPDIR/responses" |
        grep -ai '^Connection:' | tail -n 1 | cut -d' ' -f2-
}

# RFC 9112 section 9.6: a server which receives "close" MUST close the
# connection after the response, and SHOULD send "close" in the response.

@test "RFC 9112 9.6: the server closes the connection after a GET with close" {
    request GET /file 'Connection: close\r\n'
    run -0 exchange "$SERVER_PORT"
    assert_output $'200\nclosed'
    run -0 connection_header
    assert_output --regexp '^[Cc]lose$'
}

@test "RFC 9112 9.6: the server closes the connection after a HEAD with close" {
    request HEAD /file 'Connection: close\r\n'
    run -0 exchange "$SERVER_PORT"
    assert_output $'200\nclosed'
}

@test "RFC 9112 9.6: the server closes the connection after a PUT with close" {
    request PUT /put-close 'Content-Length: 5\r\nConnection: close\r\n' abcde
    run -0 exchange "$SERVER_PORT"
    assert_line --index 0 --regexp '^20[01]$'
    assert_line --index 1 closed
}

@test "RFC 9112 9.6: the server closes the connection after a DELETE with close" {
    printf 'x' > "$SERVER_DATA/delete-close"
    request DELETE /delete-close 'Connection: close\r\n'
    run -0 exchange "$SERVER_PORT"
    assert_output $'200\nclosed'
}

@test "RFC 9112 9.6: the server closes the connection after a PROPFIND of a file with close" {
    request PROPFIND /file 'Depth: 0\r\nConnection: close\r\n'
    run -0 exchange "$SERVER_PORT"
    assert_line --index 0 207
    assert_equal "${lines[${#lines[@]} - 1]}" closed
}

@test "RFC 9112 9.6: the server closes the connection after a PROPFIND of a directory with close" {
    request PROPFIND /dir 'Depth: 1\r\nConnection: close\r\n'
    run -0 exchange "$SERVER_PORT"
    assert_line --index 0 207
    assert_equal "${lines[${#lines[@]} - 1]}" closed
}

@test "RFC 9112 9.6: the server closes the connection after a MKCOL with close" {
    request MKCOL /mkcol-close 'Connection: close\r\n'
    run -0 exchange "$SERVER_PORT"
    assert_output $'201\nclosed'
}

@test "RFC 9112 9.6: the server closes the connection after a MOVE with close" {
    printf 'x' > "$SERVER_DATA/move-close"
    request MOVE /move-close \
        "Destination: http://localhost:$SERVER_PORT/moved-close\r\nConnection: close\r\n"
    run -0 exchange "$SERVER_PORT"
    assert_output $'201\nclosed'
}

@test "RFC 9112 9.6: the server closes the connection after an OPTIONS with close" {
    request OPTIONS /file 'Connection: close\r\n'
    run -0 exchange "$SERVER_PORT"
    assert_output $'200\nclosed'
}

@test "RFC 9112 9.6: the server closes the connection after an error with close" {
    request GET /missing 'Connection: close\r\n'
    run -0 exchange "$SERVER_PORT"
    assert_output $'404\nclosed'
}

@test "RFC 9112 9.6: the server closes the connection after a PROPFIND with close over HTTPS" {
    request PROPFIND /file 'Depth: 0\r\nConnection: close\r\n'
    run -0 exchange "$SERVER_PORT" tls
    assert_line --index 0 207
    assert_equal "${lines[${#lines[@]} - 1]}" closed
}

@test "RFC 9112 9.6: the server closes the connection after a GET with close over HTTPS" {
    request GET /file 'Connection: close\r\n'
    run -0 exchange "$SERVER_PORT" tls
    assert_output $'200\nclosed'
}

# RFC 9112 section 9.6: the server MUST NOT process further requests

@test "RFC 9112 9.6: a request pipelined after a GET with close does not run" {
    request GET /file 'Connection: close\r\n'
    request GET /file
    run -0 exchange "$SERVER_PORT"
    assert_output $'200\nclosed'
}

@test "RFC 9112 9.6: a request pipelined after a PROPFIND with close does not run" {
    printf 'x' > "$SERVER_DATA/kept"
    request PROPFIND /kept 'Depth: 0\r\nConnection: close\r\n'
    request DELETE /kept
    run -0 exchange "$SERVER_PORT"
    assert_line --index 0 207
    assert_equal "${lines[${#lines[@]} - 1]}" closed
    [[ -f "$SERVER_DATA/kept" ]]
}

# RFC 9110 section 7.6.1: the options of Connection are not case-sensitive

@test "RFC 9112 9.6: the server finds a close option in capital letters" {
    request GET /file 'Connection: CLOSE\r\n'
    run -0 exchange "$SERVER_PORT"
    assert_output $'200\nclosed'
}

# RFC 9112 section 9.3: HTTP/1.1 connections persist by default, and HTTP/1.0
# connections persist only with the keep-alive option

@test "RFC 9112 9.3: an HTTP/1.1 connection persists by default" {
    request GET /file
    run -0 exchange "$SERVER_PORT"
    assert_output $'200\nopen'
}

@test "RFC 9112 9.3: an HTTP/1.0 connection closes by default" {
    request GET /file '' '' HTTP/1.0
    run -0 exchange "$SERVER_PORT"
    assert_output $'200\nclosed'
}

@test "RFC 9112 9.3: an HTTP/1.0 connection with keep-alive persists" {
    request GET /file 'Connection: keep-alive\r\n' '' HTTP/1.0
    run -0 exchange "$SERVER_PORT"
    assert_output $'200\nopen'
}

# RFC 9112 section 9.6: a close with unread data makes the TCP stack send a
# reset, and the client then loses the part of the response which it did not
# read yet. The client sends a request while the server sends a large response,
# and reads the response only after the server closes.
@test "RFC 9112 9.6: the client gets all of the response when the server closes with unread requests" {
    local status=0

    head -c 33554432 /dev/zero > "$SERVER_DATA/large"
    exec 5<> "/dev/tcp/localhost/$SERVER_PORT"
    printf 'GET /large HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n' >&5
    sleep 0.05
    printf 'GET /file HTTP/1.1\r\nHost: localhost\r\n\r\n' >&5
    sleep 0.5
    cat <&5 > "$BATS_TEST_TMPDIR/response" 2> "$BATS_TEST_TMPDIR/error" || status=$?
    exec 5<&-
    run -0 cat "$BATS_TEST_TMPDIR/error"
    assert_output ''
    assert_equal "$status" 0
    assert_equal "$(tail -c 33554432 "$BATS_TEST_TMPDIR/response" | tr -d '\0' | wc -c | tr -d ' ')" 0
    (( $(wc -c < "$BATS_TEST_TMPDIR/response") > 33554432 ))
}
