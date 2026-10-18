#!/usr/bin/env bash

# Request smuggling: requests with ambiguous framing, each followed on the same
# connection by a second, smuggled request. The server must reject the first
# one and must not execute the second one. Each test uses a new connection,
# and the tests share one server.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl python3
    start_server server server http server.cfg
    mkdir "$SERVER_DATA/smuggling"
}

teardown_file() {
    stop_servers
}

setup() {
    D="smuggling/t${BATS_TEST_NUMBER}"
    mkdir "$SERVER_DATA/$D"
}

# Send the request (printf format with %s for the directory) over a new TCP
# connection, and print the first line of the response
send() {
    # shellcheck disable=SC2059 # the first argument is the format
    printf "$1" "$D" "$D" | python3 "$BATS_TEST_DIRNAME/raw_tcp_status.py" localhost "$SERVER_PORT"
}

# Print the HTTP status of a GET of the smuggled file
smuggled() {
    curl --silent -o /dev/null --write-out '%{http_code}' "$SERVER_HTTP/$D/$1"
}

readonly CL_TE='PUT /%s/cl_te HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\nPUT /%s/POLL HTTP/1.1\r\nHost: x\r\nContent-Length: 4\r\n\r\nWWWW'
readonly TE_CL='PUT /%s/te_cl HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\nPUT /%s/POLL HTTP/1.1\r\nHost: x\r\nContent-Length: 4\r\n\r\nWWWW'
readonly DUAL_CL='PUT /%s/dual_cl HTTP/1.1\r\nHost: x\r\nContent-Length: 4\r\nContent-Length: 100\r\n\r\nWWWWPUT /%s/POLL HTTP/1.1\r\nHost: x\r\nContent-Length: 4\r\n\r\nWWWW'
readonly CL_NEG='PUT /%s/cl_neg HTTP/1.1\r\nHost: x\r\nContent-Length: -1\r\n\r\nPUT /%s/POLL HTTP/1.1\r\nHost: x\r\nContent-Length: 4\r\n\r\nWWWW'
readonly TE_SUBS='PUT /%s/te_subs HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunkedX\r\n\r\nPUT /%s/POLL HTTP/1.1\r\nHost: x\r\nContent-Length: 4\r\n\r\nWWWW'
readonly TE_UPPER='PUT /%s/te_upper HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: CHUNKED\r\n\r\nPUT /%s/POLL HTTP/1.1\r\nHost: x\r\nContent-Length: 4\r\n\r\nWWWW'

# RFC 9112 section 6.1 forbids Content-Length together with Transfer-Encoding
@test "Content-Length followed by Transfer-Encoding chunked is rejected with 400" {
    run -0 send "$CL_TE"
    assert_output 'HTTP/1.1 400 Bad Request'
}

@test "a request after Content-Length and Transfer-Encoding chunked does not run" {
    send "$CL_TE"
    run -0 smuggled POLL
    assert_output 404
}

@test "Transfer-Encoding chunked followed by Content-Length is rejected with 400" {
    run -0 send "$TE_CL"
    assert_output 'HTTP/1.1 400 Bad Request'
}

@test "a request after Transfer-Encoding chunked and Content-Length does not run" {
    send "$TE_CL"
    run -0 smuggled POLL
    assert_output 404
}

# RFC 7230 section 3.3.3 rule 4: the length of the body is ambiguous
@test "two different Content-Length headers are rejected with 400" {
    run -0 send "$DUAL_CL"
    assert_output 'HTTP/1.1 400 Bad Request'
}

@test "a request after two different Content-Length headers does not run" {
    send "$DUAL_CL"
    run -0 smuggled POLL
    assert_output 404
}

# RFC 9112 section 6.2 requires digits only
@test "a negative Content-Length is rejected with 400" {
    run -0 send "$CL_NEG"
    assert_output 'HTTP/1.1 400 Bad Request'
}

@test "a request after a negative Content-Length does not run" {
    send "$CL_NEG"
    run -0 smuggled POLL
    assert_output 404
}

# A comparison of a substring took chunkedX for chunked
@test "Transfer-Encoding chunkedX is rejected with 400" {
    run -0 send "$TE_SUBS"
    assert_output 'HTTP/1.1 400 Bad Request'
}

@test "a request after Transfer-Encoding chunkedX does not run" {
    send "$TE_SUBS"
    run -0 smuggled POLL
    assert_output 404
}

# A comparison which depended on case dropped CHUNKED. Now the server reads
# the smuggled request as the body, thus no response line is certain.
@test "a request after Transfer-Encoding CHUNKED does not run" {
    send "$TE_UPPER" > /dev/null
    run -0 smuggled POLL
    assert_output 404
}
