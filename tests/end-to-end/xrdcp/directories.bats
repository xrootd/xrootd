#!/usr/bin/env bash

load ../setup/server.bash

# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=45
export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPDISABLEX509=1 X509_USER_PROXY=/dev/null/x509
export no_proxy=localhost,127.0.0.1 NO_PROXY=localhost,127.0.0.1

setup() {
    cd "$BATS_TEST_TMPDIR"
}

teardown() {
    stop_servers
}

http_server() {
    require_plugin libXrdHttp
    require_plugin libXrdClHttp
    start_server http.cfg
    mkdir -p "$SERVER_DATA/tree/nested"
}

@test "parallel uploads to an HTTP directory keep the names of both files" {
    http_server
    printf 'first\n' > one
    printf 'second\n' > two
    xrdcp --parallel 2 one two "$SERVER_HTTP/tree"
    run -0 cmp one "$SERVER_DATA/tree/one"
    run -0 cmp two "$SERVER_DATA/tree/two"
}

check_tree() {
    http_server
    printf 'top\n' > "$SERVER_DATA/tree/top"
    printf 'leaf\n' > "$SERVER_DATA/tree/nested/leaf"
    mkdir download
    xrdcp --recursive "$SERVER_HTTP/tree$1" download
    run -0 cmp "$SERVER_DATA/tree/top" download/tree/top
    run -0 cmp "$SERVER_DATA/tree/nested/leaf" download/tree/nested/leaf
}

@test "a recursive HTTP download preserves the directory tree" {
    check_tree ''
}

@test "a recursive HTTP download accepts a directory with a trailing slash" {
    check_tree '/'
}

@test "a slash in the source query leaves the download directory name unchanged" {
    check_tree '?test=directory/token'
}

@test "a directory with a trailing slash and a query downloads under its name" {
    check_tree '/?test=directory/token'
}

@test "a recursive XRootD download preserves hash and backslash in file names" {
    start_server
    mkdir "$SERVER_DATA/source" download
    printf 'hash\n' > "$SERVER_DATA/source/run#1.root"
    printf 'backslash\n' > "$SERVER_DATA/source/run\1.root"
    xrdcp --recursive "$SERVER_URL//source" download
    run -0 cmp "$SERVER_DATA/source/run#1.root" 'download/source/run#1.root'
    run -0 cmp "$SERVER_DATA/source/run\1.root" 'download/source/run\1.root'
}
