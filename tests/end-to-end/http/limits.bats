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

# RFC 9112 section 2.2: a server SHOULD ignore at least one empty line before
# a request line. Each empty line also keeps the connection active, thus the
# server accepts only 10 of them.

@test "a request after 10 empty lines gets a response" {
    run -0 send '\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\nGET /file HTTP/1.1\r\nHost: x\r\n\r\n'
    assert_output 'HTTP/1.1 200 OK'
}

@test "a request after 11 empty lines does not run" {
    run -0 send '\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\nGET /file HTTP/1.1\r\nHost: x\r\n\r\n'
    assert_output '<server closed the connection without sending a status line>'
}

# The count starts again at each request
@test "a pipelined request after 10 empty lines gets a response" {
    exec 5<> "/dev/tcp/localhost/$SERVER_PORT"
    printf 'GET /file HTTP/1.1\r\nHost: x\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\nGET /file HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n' >&5
    # The body of the file has no newline, thus the next status line can
    # follow it on the same line
    run -0 grep -ao 'HTTP/1.1 200' <&5
    exec 5<&-
    assert_output $'HTTP/1.1 200\nHTTP/1.1 200'
}

# The server keeps each header line of a request. Without a limit, a request
# with many header lines uses all the memory of the server. RFC 6585 section 5
# gives 431 for header fields which are too large.

# headers <count> [extra]: print the format of a GET with <count> header
# lines of 1000 bytes each, and the extra header lines
headers() {
    local pad request="GET /file HTTP/1.1\\r\\nHost: x\\r\\n${2:-}" i

    printf -v pad '%*s' 990 ''
    for ((i = 0; i < $1; i++)); do
        request+="X-$i: ${pad// /x}\r\n"
    done
    printf '%s' "$request\r\n"
}

@test "a request with 60 KB of header lines gets a response" {
    run -0 send "$(headers 60)"
    assert_output 'HTTP/1.1 200 OK'
}

@test "a request with 70 KB of header lines gets 431" {
    run -0 send "$(headers 70)"
    assert_output --partial ' 431 '
}

# The count starts again at each request
@test "two pipelined requests with 60 KB of header lines each get two responses" {
    exec 5<> "/dev/tcp/localhost/$SERVER_PORT"
    # shellcheck disable=SC2059 # the format comes from headers
    printf "$(headers 60)$(headers 60 'Connection: close\r\n')" >&5
    run -0 grep -ao 'HTTP/1.1 [0-9]*' <&5
    exec 5<&-
    assert_output $'HTTP/1.1 200\nHTTP/1.1 200'
}

# A NUL byte ends the copy of a line, thus the line can be empty. The check
# of an empty line before the request line must not read past its end. The
# first bytes of a connection must look like a request, thus the line comes
# after a request.

@test "the server stays up after a line with only a NUL byte" {
    send 'GET /file HTTP/1.1\r\nHost: x\r\n\r\n\0\n'
    run -0 status
    assert_output 200
}
