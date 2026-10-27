#!/usr/bin/env bash

# Reads over HTTP through the cache. The tests share the origin, the cache
# and the collector; each test has its own file.

load cache.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    start_cache http
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    F="t${BATS_TEST_NUMBER}.dat"
    make_origin_file "$F" 8
}

@test "a GET through the cache has the content of the origin" {
    curl --silent --fail -o out.dat "$CACHE_HTTP/$F"
    run -0 cmp "$ORIGIN_DATA/$F" out.dat
}

@test "a GET through the cache fetches the whole file" {
    curl --silent --fail -o out.dat "$CACHE_HTTP/$F"
    run -0 close_field "$F" 1 b_todisk
    assert_output $((8 * BLOCK))
}

@test "a GET of a range through the cache returns the range" {
    curl --silent --fail -r 4096-8191 -o out.dat "$CACHE_HTTP/$F"
    origin_range 4096:4096 "$F" > expected.dat
    run -0 cmp expected.dat out.dat
}

@test "a GET through the cache of a file which does not exist returns 404" {
    run -0 curl --silent -o /dev/null -w '%{http_code}' "$CACHE_HTTP/$F.missing"
    assert_output 404
}
