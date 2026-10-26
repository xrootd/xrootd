#!/usr/bin/env bash

# xrdfs stat over HTTP, with the XrdClHttp plugin, through a redirect: the
# server "origin" redirects stat to the server "target". The client has one
# worker thread, which must find the capabilities of the target after the
# redirect. Each test starts its own servers, as the tests read the server
# logs.

load ../../setup/server.bash

# BATS stops a test and its processes after this time, thus a client which
# hangs after the redirect fails its test
# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=60

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPNUMTHREADS=1

# The client uses no proxy, token or certificate of the user
export XRD_HTTPDISABLEX509=1 X509_USER_PROXY=/dev/null/x509
export no_proxy=localhost,127.0.0.1 NO_PROXY=localhost,127.0.0.1
unset BEARER_TOKEN BEARER_TOKEN_FILE XRD_HTTPHEADERS
unset XRD_HTTPCLIENTCERTFILE XRD_HTTPCLIENTKEYFILE

setup() {
    require_plugin libXrdHttp
    require_plugin libXrdClHttp
    start_server target server http
    printf 'redirected file!\n' > "$TARGET_DATA/file"
    printf 'xrootd.redirect localhost:%s stat\n' "$TARGET_PORT" > "$BATS_TEST_TMPDIR/redirect.cfg"
    start_server origin server http "$BATS_TEST_TMPDIR/redirect.cfg"
}

teardown() {
    stop_servers
}

@test "stat over HTTP gets the size of a file through a redirect" {
    run -0 xrdfs "$ORIGIN_HTTP" stat /file
    assert_output --partial 'Size:   17'
}

@test "stat sends OPTIONS to the origin" {
    xrdfs "$ORIGIN_HTTP" stat /file
    run -0 grep -F 'got hdr line: OPTIONS / HTTP/1.1' "$ORIGIN_LOG"
}

@test "stat sends PROPFIND to the origin" {
    xrdfs "$ORIGIN_HTTP" stat /file
    run -0 grep -F 'got hdr line: PROPFIND /file HTTP/1.1' "$ORIGIN_LOG"
}

@test "the origin redirects stat to the target" {
    xrdfs "$ORIGIN_HTTP" stat /file
    run -0 grep -F "Redirecting to Location: http://localhost:$TARGET_PORT/file" "$ORIGIN_LOG"
}

@test "stat sends OPTIONS to the target after the redirect" {
    xrdfs "$ORIGIN_HTTP" stat /file
    run -0 grep -F 'got hdr line: OPTIONS / HTTP/1.1' "$TARGET_LOG"
}

@test "stat sends PROPFIND to the target after the redirect" {
    xrdfs "$ORIGIN_HTTP" stat /file
    run -0 grep -F 'got hdr line: PROPFIND /file HTTP/1.1' "$TARGET_LOG"
}
