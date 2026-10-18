#!/usr/bin/env bash

# Static headers of the configuration (http.staticheader), for all verbs or
# for one verb. The tests share one server.

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

# The response headers of a request with the given curl arguments, without CR
headers() {
    curl --silent -D - -o /dev/null "$@" "$SERVER_HTTP/alphabet.txt" | tr -d '\r'
}

@test "OPTIONS has the header of the OPTIONS verb" {
    run -0 headers -X OPTIONS
    assert_line 'Access-Control-Allow-Origin: *'
}

@test "OPTIONS has the header of all verbs" {
    run -0 headers -X OPTIONS
    assert_line 'Test: 1'
}

@test "GET has the first value of a header of the GET verb once" {
    run -0 bats_pipe headers \| grep -c '^Foo: Bar$'
    assert_output 1
}

@test "GET has the second value of a header of the GET verb once" {
    run -0 bats_pipe headers \| grep -c '^Foo: Baz$'
    assert_output 1
}

@test "GET has the header of all verbs once" {
    run -0 bats_pipe headers \| grep -c '^Test: 1$'
    assert_output 1
}

@test "HEAD has the header of all verbs once" {
    run -0 bats_pipe headers -I \| grep -c '^Test: 1$'
    assert_output 1
}
