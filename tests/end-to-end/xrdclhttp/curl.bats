#!/usr/bin/env bash

# Requests of curl and xrdfs to the HTTPS cache of xrdclhttp.bash, which
# reads the HTTPS origin with XrdClHttp. The tests share the servers.

load xrdclhttp.bash

setup_file() {
    require_commands curl
    start_https_cache
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
}

# Print the HTTP status of a GET of the URL with the token
status() {
    curl --silent -L --cacert "$CREDENTIALS/ca.pem" -o body.txt -w '%{http_code}' \
        -H "Authorization: Bearer $2" "$1"
}

@test "a GET through the cache with a token returns the file of the origin" {
    run -0 status "$CACHE_HTTPS/test/hello_world.txt" "$(<"$READ_TOKEN")"
    assert_output 200
    assert_equal "$(<body.txt)" 'Hello, World'
}

@test "a GET through the cache with an invalid token is forbidden" {
    run -0 status "$CACHE_HTTPS/test/hello_world.txt" missing
    assert_output 403
}

@test "a GET through the cache of a file which does not exist returns 404" {
    run -0 status "$CACHE_HTTPS/test/missing.txt" "$(<"$READ_TOKEN")"
    assert_output 404
}

@test "a GET through the cache of a public directory returns 200" {
    run -0 curl --silent -L --cacert "$CREDENTIALS/ca.pem" -o body.txt -w '%{http_code}' \
        "$CACHE_HTTPS/test-public/subdir"
    assert_output 200
}

@test "xrdfs ls -l over roots through the cache lists the entries of the origin" {
    run -0 --separate-stderr xrdfs "roots://localhost:$CACHE_PORT" ls -l /test-public/subdir
    assert_equal "${#lines[@]}" 3
}

@test "xrdfs ls -l through the cache reports the mode and the size of a file" {
    run -0 --separate-stderr xrdfs "roots://localhost:$CACHE_PORT" ls -l /test-public/subdir
    assert_line --regexp '^-r-----r-- .* 14 .* /test-public/subdir/test2$'
}

@test "xrdfs ls -l through the cache reports the mode of a directory" {
    run -0 --separate-stderr xrdfs "roots://localhost:$CACHE_PORT" ls -l /test-public/subdir
    assert_line --regexp '^dr-x---r-x .* /test-public/subdir/test3$'
}
