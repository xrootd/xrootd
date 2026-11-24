#!/usr/bin/env bash

# HTTP/1.1 lets a client send several requests on a connection without a wait
# for each response (pipelining). The server must answer all of them in order,
# also when the requests arrive together in one read. The pull of XrdHttpTPC
# with more than one stream sent its requests in this way with libcurl before
# 7.62.0. The tests share the servers: a data server, and a redirector, which
# ends each request with a redirect.

load ../setup/server.bash

# a server which keeps a connection open must not block the run
# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=60

setup_file() {
    require_plugin libXrdHttp
    cd "$BATS_FILE_TMPDIR"

    start_server http.cfg tls.cfg checksum.cfg
    start_server rdr http.cfg pipeline-rdr.cfg

    printf '0123456789' > "$SERVER_DATA/file"
}

setup() {
    requests=
}

teardown_file() {
    stop_servers
}

# Append a request for the given method and path to $requests. The extra
# headers may use \r\n escapes. The last request of a pipeline closes the
# connection, so that the read of the responses ends.
request() {
    local method=$1 path=$2 extra=${3:-} body=${4:-} r

    printf -v r '%s %s HTTP/1.1\r\nHost: localhost\r\n%b\r\n%s' "$method" "$path" "$extra" "$body"
    requests+=$r
}

# Print what the server sends on the connection of descriptor 5, until it
# closes the connection, or sends nothing for 10 s. BATS uses the descriptors
# 3 and 4, and macOS has no timeout(1), thus read -t.
_receive() {
    local line=

    while IFS= read -r -t 10 line <&5 || [[ -n "$line" ]]; do
        printf '%s\n' "$line"
        line=
    done
}

# Append <count> GET requests of 1001 bytes each to $requests. With more than
# 1047 of them, the requests do not fit into the buffer of the server (1 MB).
large_requests() {
    local pad

    printf -v pad '%*s' 953 ''
    for ((i = 0; i < $1; i++)); do
        request GET /file "X-Pad: ${pad// /x}\r\n"
    done
}

# pipeline <port> [tls]
#
# Send $requests and print the status codes received before the server closes
# the connection, or before a timeout. A status line may follow a response
# body without a newline, thus grep -o rather than a line match. The file
# $BATS_TEST_TMPDIR/responses keeps all that the server sent.
pipeline() {
    local responses=$BATS_TEST_TMPDIR/responses

    if [[ ${2:-} == tls ]]; then
        # -quiet also keeps the connection open at the end of the input
        printf '%s' "$requests" > "$BATS_TEST_TMPDIR/requests"
        openssl s_client -quiet -connect "localhost:$1" \
            < "$BATS_TEST_TMPDIR/requests" > "$responses" 2> /dev/null || true
    else
        # Write while the responses arrive, as a large pipeline does not fit
        # into the buffers of the sockets
        exec 5<>"/dev/tcp/localhost/$1"
        printf '%s' "$requests" >&5 &
        _receive > "$responses"
        wait "$!" || true
        exec 5<&-
    fi
    grep -ao 'HTTP/1\.1 [0-9]\{3\}' "$responses" | cut -d' ' -f2 || true
}

# Print the body of each response of the last pipeline, one line for each
# body. A body ends where the status line of the next response starts. A body
# without data, such as the body of a HEAD response, gives an empty line.
bodies() {
    tr -d '\r' < "$BATS_TEST_TMPDIR/responses" | awk '
        /^$/ && !body { body = 1; next }
        body {
            i = index($0, "HTTP/1.1 ")
            if (i) { print substr($0, 1, i - 1); body = 0 } else print
        }'
}

@test "two pipelined GET requests get two responses" {
    request GET /file 'Range: bytes=0-4\r\n'
    request GET /file 'Range: bytes=5-9\r\nConnection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'206\n206'
    run -0 bodies
    assert_output $'01234\n56789'
}

@test "sixteen pipelined GET requests get sixteen responses" {
    for _ in {1..15}; do
        request GET /file 'Range: bytes=0-4\r\n'
    done
    request GET /file 'Range: bytes=0-4\r\nConnection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_equal "$(grep -c '^206$' <<< "$output")" 16
}

@test "a GET pipelined after a HEAD request gets a response" {
    request HEAD /file
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'200\n200'
}

# The server sent the response to the HEAD, and kept the GET in its buffer
@test "a GET pipelined after a HEAD with Want-Digest gets a response" {
    request HEAD /file 'Want-Digest: adler32\r\n'
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'200\n200'
    run -0 grep -ai '^Digest: adler32=' "$BATS_TEST_TMPDIR/responses"
    run -0 bodies
    assert_output $'\n0123456789'
}

@test "a GET pipelined after a DELETE request gets a response" {
    printf 'x' > "$SERVER_DATA/removed"
    request DELETE /removed
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'200\n200'
}

# The server answers OPTIONS and DELETE without a read of the body, and must
# skip it: else the body runs as a request (request smuggling).
@test "a request in the body of an OPTIONS request does not run" {
    local body=$'GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n'
    request OPTIONS /file "Content-Length: ${#body}\r\n" "$body"
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'200\n200'
}

@test "a GET pipelined after a DELETE with a body gets a response" {
    printf 'x' > "$SERVER_DATA/removed-body"
    request DELETE /removed-body 'Content-Length: 3\r\n' abc
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'200\n200'
}

# X-Transfer-Status asks for a chunked response, and the request has no
# chunked body. Thus the server must skip the body of the request.
@test "a request in the body of a GET with X-Transfer-Status does not run" {
    local body=$'DELETE /victim-status HTTP/1.1\r\nHost: localhost\r\n\r\n'
    printf 'x' > "$SERVER_DATA/victim-status"
    request GET /file "Content-Length: ${#body}\r\nX-Transfer-Status: true\r\nTE: trailers\r\n" "$body"
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'200\n200'
    [[ -f "$SERVER_DATA/victim-status" ]]
}

# The body has the form of a chunk size. Read as a chunked body, the PUT
# takes 4096 bytes, also the next request on the connection. Behind a proxy
# which frames by Content-Length, that can be the request of another user.
@test "a PUT with X-Transfer-Status reads its body by Content-Length" {
    request PUT /xts 'Content-Length: 6\r\nX-Transfer-Status: true\r\n' $'1000\r\n'
    request PUT /xts-next 'Content-Length: 4\r\nConnection: close\r\n' WWWW
    run -0 pipeline "$SERVER_PORT"
    assert_output --regexp $'^20[01]\n20[01]$'
    assert_equal "$(od -An -c "$SERVER_DATA/xts" | tr -s ' ')" ' 1 0 0 0 \r \n'
    assert_equal "$(cat "$SERVER_DATA/xts-next")" WWWW
}

# RFC 9112 section 2.2: a server SHOULD ignore an empty line before a request
@test "an empty line before a pipelined request is ignored" {
    request GET /file
    requests+=$'\r\n'
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'200\n200'
}

@test "a GET pipelined after an OPTIONS request gets a response" {
    request OPTIONS /file
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'200\n200'
}

@test "a GET pipelined after a PUT gets a response" {
    request PUT /new 'Content-Length: 5\r\n' abcde
    request GET /new 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_line --index 0 --regexp '^20[01]$'
    assert_line --index 1 '200'
    run -0 bodies
    assert_line --index 1 abcde
}

# The requests and the body of the PUT are more than the buffer of the server
# holds (1 MB). The server reads them while it sends the responses.
@test "a PUT pipelined after 1 MB of requests stores its body" {
    local body

    large_requests 1040
    body=$(seq 20000 | tr -d '\n' | head -c 65536)
    request PUT /large 'Content-Length: 65536\r\n' "$body"
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_equal "${#lines[@]}" 1042
    assert_line --index 1040 --regexp '^20[01]$'
    assert_line --index 1041 '200'
    assert_equal "$(cat "$SERVER_DATA/large")" "$body"
}

# The body of a PROPFIND starts 20 bytes before the end of the buffer of the
# server (1 MB). The server reads the rest of the body before it parses it, and
# the body must be one block. This position holds only if the buffer does not
# become empty: a read which ends at the end of a header line empties it.
# Thus the test found the bug of a ring buffer only when the server read slowly.
@test "a PROPFIND pipelined after 1 MB of requests gets its body" {
    local body='<?xml version="1.0"?><propfind xmlns="DAV:"><allprop/></propfind>'
    local propfind pad

    large_requests 1045
    printf -v propfind 'PROPFIND /file HTTP/1.1\r\nHost: localhost\r\nDepth: 0\r\nContent-Length: %d\r\n\r\n' "${#body}"
    # A GET of 48 bytes and the padding, up to 20 bytes before 1 MB
    printf -v pad '%*s' $((1048576 - 20 - ${#requests} - ${#propfind} - 48)) ''
    request GET /file "X-Pad: ${pad// /x}\r\n"
    requests+=$propfind$body
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    # The body of the 207 response has status lines too
    assert_line --index 1046 '207'
    assert_equal "${lines[${#lines[@]} - 1]}" '200'
}

# A read of the server can take only a part of a TLS record. The rest waits
# in the TLS layer, and the poller does not see it.
@test "a PUT pipelined after 1 MB of requests over HTTPS stores its body" {
    local body

    require_commands openssl
    large_requests 1040
    body=$(seq 20000 | tr -d '\n' | head -c 65536)
    request PUT /large-tls 'Content-Length: 65536\r\n' "$body"
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT" tls
    assert_equal "${#lines[@]}" 1042
    assert_line --index 1040 --regexp '^20[01]$'
    assert_line --index 1041 '200'
    assert_equal "$(cat "$SERVER_DATA/large-tls")" "$body"
}

# A body of 40 KB is in three TLS records, and one read over TLS returns at
# most one record
@test "a PROPFIND over HTTPS gets a body in more than one TLS record" {
    local body pad

    require_commands openssl
    printf -v pad '%*s' 40000 ''
    body="<?xml version=\"1.0\"?><propfind xmlns=\"DAV:\">$pad<allprop/></propfind>"
    request PROPFIND /file "Depth: 0\r\nContent-Length: ${#body}\r\n" "$body"
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT" tls
    # The body of the 207 response has status lines too
    assert_line --index 0 '207'
    assert_equal "${lines[${#lines[@]} - 1]}" '200'
}

@test "two pipelined GET requests get two responses over HTTPS" {
    require_commands openssl
    request GET /file 'Range: bytes=0-4\r\n'
    request GET /file 'Range: bytes=5-9\r\nConnection: close\r\n'
    run -0 pipeline "$SERVER_PORT" tls
    assert_output $'206\n206'
    run -0 bodies
    assert_output $'01234\n56789'
}

@test "a redirector redirects each GET of a pipeline" {
    request GET /file
    request GET /file
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$RDR_PORT"
    assert_output $'302\n302\n302'
}

# The redirector answers the PUT before it reads the body
@test "a redirector skips the body of a redirected PUT" {
    request PUT /new 'Content-Length: 5\r\n' abcde
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$RDR_PORT"
    assert_line --index 0 --regexp '^30[27]$'
    assert_line --index 1 '302'
    assert_equal "${#lines[@]}" 2
}

# The redirector answered the second GET with 416
@test "a redirector redirects each GET with a range of a pipeline" {
    request GET /file 'Range: bytes=0-4\r\n'
    request GET /file 'Range: bytes=5-9\r\n'
    request GET /file 'Range: bytes=0-4\r\nConnection: close\r\n'
    run -0 pipeline "$RDR_PORT"
    assert_output $'302\n302\n302'
}

@test "a redirector redirects each HEAD of a pipeline" {
    request HEAD /file
    request HEAD /file
    request HEAD /file 'Connection: close\r\n'
    run -0 pipeline "$RDR_PORT"
    assert_output $'307\n307\n307'
}

# RFC 9110 section 10.1.1: after Expect: 100-continue, a client which gets a
# final response and no 100 Continue can send the body or not. The server
# cannot find the start of the next request, thus it must close.

@test "a redirector closes the connection after a PUT with Expect" {
    request PUT /new 'Content-Length: 5\r\nExpect: 100-continue\r\n'
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$RDR_PORT"
    assert_output 307
    run -0 grep -ai '^Connection: close' "$BATS_TEST_TMPDIR/responses"
}

@test "a data server closes the connection after an OPTIONS with Expect" {
    request OPTIONS /file 'Content-Length: 5\r\nExpect: 100-continue\r\n'
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output 200
    run -0 grep -ai '^Connection: close' "$BATS_TEST_TMPDIR/responses"
}

# The client sends the body at once. The server cannot know it: the body can
# also be a next request.
@test "a request in the body of an OPTIONS with Expect does not run" {
    local body=$'DELETE /victim-expect HTTP/1.1\r\nHost: localhost\r\n\r\n'
    printf 'x' > "$SERVER_DATA/victim-expect"
    request OPTIONS /file "Content-Length: ${#body}\r\nExpect: 100-continue\r\n" "$body"
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output 200
    [[ -f "$SERVER_DATA/victim-expect" ]]
}

@test "a GET pipelined after a PUT with 100 Continue gets a response" {
    request PUT /continued 'Content-Length: 5\r\nExpect: 100-continue\r\n' abcde
    request GET /continued 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'100\n201\n200'
    assert_equal "$(bodies | tail -n 1)" abcde
}

# RFC 9112 section 7.1: a chunked body ends with its last chunk. The server
# cannot skip a chunked body which the method does not read.

@test "a DELETE with a chunked body closes the connection" {
    local chunk=$'DELETE /victim-delete HTTP/1.1\r\nHost: localhost\r\n\r\n' body
    printf 'x' > "$SERVER_DATA/removed-chunked"
    printf 'x' > "$SERVER_DATA/victim-delete"
    printf -v body '%x\r\n%s\r\n0\r\n\r\n' "${#chunk}" "$chunk"
    request DELETE /removed-chunked 'Transfer-Encoding: chunked\r\n' "$body"
    request GET /file 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output 200
    run -0 grep -ai '^Connection: close' "$BATS_TEST_TMPDIR/responses"
    [[ -f "$SERVER_DATA/victim-delete" ]]
}

@test "a GET pipelined after a chunked PUT gets a response" {
    request PUT /chunked 'Transfer-Encoding: chunked\r\n' $'5\r\nabcde\r\n0\r\n\r\n'
    request GET /chunked 'Connection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output $'201\n200'
    assert_equal "$(bodies | tail -n 1)" abcde
}

# The parser rejects a request with both a chunked body and a Content-Length,
# but X-Transfer-Status asks only for a chunked response
@test "a GET with X-Transfer-Status before a Content-Length gets a response" {
    request GET /file 'X-Transfer-Status: true\r\nTE: trailers\r\nContent-Length: 0\r\nConnection: close\r\n'
    run -0 pipeline "$SERVER_PORT"
    assert_output 200
}
