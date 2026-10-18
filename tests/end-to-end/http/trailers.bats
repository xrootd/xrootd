#!/usr/bin/env bash

# The X-Transfer-Status trailer, which reports the end of a transfer, and
# read failures with and without it. The fault injection of libXrdOssTests
# fails every read of fail_read.txt after offset 0. The tests share one
# server.

load ../setup/server.bash

readonly SIZE=4194304

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    start_server server server http server.cfg
    printf 'abcdefghijklmnopqrstuvw987' > "$SERVER_DATA/alphabet.txt"
    printf "%1048576s" " " | sed 's/ /blah/g' > "$SERVER_DATA/fail_read.txt"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
}

@test "a GET with trailers reports a complete transfer" {
    run -0 bats_pipe curl --silent --raw -H 'X-Transfer-Status: true' -H 'TE: trailers' \
        "$SERVER_HTTP/alphabet.txt" \| tr -d '\r'
    assert_line 'X-Transfer-Status: 200: OK'
}

@test "a GET of a file which fails to read fails without trailers" {
    run ! curl --silent --raw "$SERVER_HTTP/fail_read.txt" --output /dev/null
}

@test "a GET of a file which fails to read starts with the status 200" {
    run curl --silent --raw "$SERVER_HTTP/fail_read.txt" --output /dev/null --write-out '%{http_code}'
    assert_output 200
}

@test "a GET of a file which fails to read returns less than the file" {
    run curl --silent --raw "$SERVER_HTTP/fail_read.txt" --output /dev/null --write-out '%{size_download}'
    refute_output "$SIZE"
}

# Two requests on one connection, with trailers: each one ends with the error
# in the trailer, and the connection stays usable.
keepalive() {
    curl -v --raw -H 'TE: trailers' -H 'Connection: Keep-Alive' -H 'X-Transfer-Status: true' \
        "$SERVER_HTTP/fail_read.txt?try=1" -v "$SERVER_HTTP/fail_read.txt?try=2" > body.txt 2> stderr.txt
}

@test "each GET with trailers of a file which fails to read reports the read error" {
    keepalive || true
    run -0 bats_pipe grep -B 1 'X-Transfer-Status: 500: Unable to read' body.txt \| grep -c -E '^0'
    assert_output 2
}

@test "a GET with trailers of a file which fails to read has a correct framing" {
    keepalive || true
    run ! grep -F 'Leftovers after chunking' stderr.txt
}

@test "a GET with trailers of a file which fails to read keeps the connection" {
    keepalive || true
    run ! grep -F 'Connection died' stderr.txt
}
