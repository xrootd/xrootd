#!/usr/bin/env bash

# HTTP/1.1 pipelining: a client sends the next request before it gets the
# response to the last one, thus the server finds the next request in its
# buffer when a request ends. The pull of XrdHttpTPC with more than one
# stream sends its requests in this way with libcurl before 7.62.0. The tests
# use a redirector, which ends each request with a redirect.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands python3
    cd "$BATS_FILE_TMPDIR"

    start_server rdr http.cfg pipeline-rdr.cfg
}

teardown_file() {
    stop_servers
}

# pipeline <port> <count> <request>
#
# Send <count> copies of the request (a printf format) on one connection, in
# one write, and print the status of each response
pipeline() {
    local port=$1 count=$2 request=$3 i

    for (( i = 0; i < count; i++ )); do
        # shellcheck disable=SC2059 # the argument is the format
        printf "$request"
    done | python3 "$BATS_TEST_DIRNAME/pipeline.py" localhost "$port" "$count"
}

readonly GET='GET /file HTTP/1.1\r\nHost: localhost\r\n\r\n'
readonly GET_RANGE='GET /file HTTP/1.1\r\nHost: localhost\r\nRange: bytes=1024-2047\r\n\r\n'
readonly HEAD='HEAD /file HTTP/1.1\r\nHost: localhost\r\n\r\n'

@test "a redirector redirects each GET of a pipeline" {
    run -0 pipeline "$RDR_PORT" 3 "$GET"
    assert_output $'302\n302\n302'
}

# Without the fix, the redirector answered the second GET with 416
@test "a redirector redirects each GET with a range of a pipeline" {
    run -0 pipeline "$RDR_PORT" 3 "$GET_RANGE"
    assert_output $'302\n302\n302'
}

@test "a redirector redirects each HEAD of a pipeline" {
    run -0 pipeline "$RDR_PORT" 3 "$HEAD"
    assert_output $'307\n307\n307'
}
