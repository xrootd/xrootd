#!/usr/bin/env bash

# GET requests with several byte ranges, which the server answers with a
# multipart body. The tests share one server, with a file of 26 bytes.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    start_server server server http server.cfg
    printf 'abcdefghijklmnopqrstuvw987' > "$SERVER_DATA/alphabet.txt"
}

teardown_file() {
    stop_servers
}

# The multipart response to the ranges 0-3 and 24-26, without CR
ranges() {
    curl --silent -H 'range: bytes=0-3,24-26' "$SERVER_HTTP/alphabet.txt" | tr -d '\r'
}

@test "the first part has the content range of the first range" {
    run -0 ranges
    assert_line 'Content-range: bytes 0-3/26'
}

@test "the first part has the bytes of the first range" {
    run -0 ranges
    assert_line 'abcd'
}

@test "the second part ends at the end of the file" {
    run -0 ranges
    assert_line 'Content-range: bytes 24-25/26'
}

@test "the second part has the last bytes of the file" {
    run -0 ranges
    assert_line '87'
}

@test "the body has three boundary delimiters" {
    run -0 bats_pipe ranges \| grep -cF -- '--123456'
    assert_output 3
}
