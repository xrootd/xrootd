#!/usr/bin/env bash

load ../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"

teardown() {
    stop_servers
}

@test "HTTP directory copy uploads multiple files and downloads nested paths" {
    require_plugin libXrdHttp
    require_plugin libXrdClHttp
    start_server http.cfg
    cd "$BATS_TEST_TMPDIR"

    printf 'first HTTP directory copy\n' > multi-one.ref
    printf 'second HTTP directory copy\n' > multi-two.ref
    printf 'nested HTTP directory copy\n' > multi-three.ref
    xrdfs "$SERVER_HTTP" mkdir -p /multi-upload/nested
    xrdcp --parallel 2 multi-one.ref multi-two.ref "$SERVER_HTTP/multi-upload"
    xrdcp multi-three.ref "$SERVER_HTTP/multi-upload/nested/multi-three.ref"

    local suffix download
    for suffix in "" "/" "?test=directory/token" "/?test=directory/token"; do
        download=$(mktemp -d "$BATS_TEST_TMPDIR/recursive.XXXXXX")
        xrdcp --recursive "$SERVER_HTTP/multi-upload$suffix" "$download"
        cmp multi-one.ref "$download/multi-upload/multi-one.ref"
        cmp multi-two.ref "$download/multi-upload/multi-two.ref"
        cmp multi-three.ref "$download/multi-upload/nested/multi-three.ref"
    done
}

@test "recursive HTTP copies preserve credentials and reject invalid listings" {
    require_plugin libXrdClHttp
    require_commands python3
    python3 "$BATS_TEST_DIRNAME/../http/http-directory.py" "$(command -v xrdcp)"
}

@test "recursive native copies preserve hash and backslash in file names" {
    start_server
    cd "$BATS_TEST_TMPDIR"

    mkdir source download
    xrdfs "$SERVER_URL" mkdir /native-remote
    local name
    for name in 'run#1.root' 'run\1.root'; do
        printf 'native name %s\n' "$name" > "source/$name"
        xrdcp -np "source/$name" "$SERVER_URL//native-remote/$name"
    done
    xrdcp --recursive "$SERVER_URL//native-remote" download
    for name in 'run#1.root' 'run\1.root'; do
        cmp "source/$name" "download/native-remote/$name"
    done
}
