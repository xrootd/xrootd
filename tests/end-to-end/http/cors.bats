#!/usr/bin/env bash

# Cross-origin resource sharing (CORS) with the plugin XrdHttpCors, which
# allows one origin. The tests share one server.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_plugin libXrdHttpCors
    require_commands curl
    start_server server server http server.cfg
    printf 'abcdefghijklmnopqrstuvw987' > "$SERVER_DATA/alphabet.txt"
}

teardown_file() {
    stop_servers
}

headers_for_origin() {
    curl --silent -D - -o /dev/null -H "Origin: $1" "$SERVER_HTTP/alphabet.txt" | tr -d '\r'
}

@test "a request from an unknown origin has no CORS header" {
    run -0 headers_for_origin does_not_exist
    refute_output --partial 'Access-Control-Allow-Origin'
}

@test "a request from the allowed origin allows that origin" {
    run -0 headers_for_origin https://webserver.bli.bla.blo
    assert_line 'Access-Control-Allow-Origin: https://webserver.bli.bla.blo'
}
