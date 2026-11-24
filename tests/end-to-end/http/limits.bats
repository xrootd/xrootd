#!/usr/bin/env bash

# Limits of the request parser. A request which is too large, or which never
# ends, must not stop the server or keep its resources. Each test has its own
# server, so that a crash cannot change the result of the next test.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl python3
}

setup() {
    start_server http.cfg
    printf '0123456789' > "$SERVER_DATA/file"
}

teardown() {
    stop_servers
}

# Send the request (printf format) over a new TCP connection, and print the
# first line of the response
send() {
    # shellcheck disable=SC2059 # the first argument is the format
    printf "$1" | python3 "$BATS_TEST_DIRNAME/raw_tcp_status.py" localhost "$SERVER_PORT"
}

# Print the HTTP status of a GET of the file on a new connection
status() {
    curl --silent -o /dev/null --write-out '%{http_code}' "$SERVER_HTTP/file"
}

# The server reads the body of a PROPFIND into its buffer (1 MB). A length
# above 2 GB - 1 did not fit into the size of the read.

@test "a PROPFIND with a body of 2 GB gets 413" {
    run -0 send 'PROPFIND /file HTTP/1.1\r\nHost: x\r\nContent-Length: 2147483648\r\n\r\n'
    assert_output --partial ' 413 '
}

@test "the server stays up after a PROPFIND with a body of 2 GB" {
    send 'PROPFIND /file HTTP/1.1\r\nHost: x\r\nContent-Length: 2147483648\r\n\r\n'
    run -0 status
    assert_output 200
}

@test "a PROPFIND with a body of 4 GB - 1 gets 413" {
    run -0 send 'PROPFIND /file HTTP/1.1\r\nHost: x\r\nContent-Length: 4294967295\r\n\r\n'
    assert_output --partial ' 413 '
}

@test "a PROPFIND with a body larger than the buffer gets 413" {
    run -0 send 'PROPFIND /file HTTP/1.1\r\nHost: x\r\nContent-Length: 2000000\r\n\r\n'
    assert_output --partial ' 413 '
}
