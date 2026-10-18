#!/usr/bin/env bash

# Transfers with several substreams and with many requests in flight. The
# tests share one server, with the file /ref.dat on it.

load ../setup/server.bash

setup_file() {
    start_server server server

    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" $(( 4 * 1024 * 1024 + 3 ))
    cp "$REF" "$SERVER_DATA/ref.dat"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    TARGET="/t${BATS_TEST_NUMBER}.dat"
}

@test "download with --streams 3 binds 3 substreams" {
    xrdcp --streams 3 "$SERVER_URL//ref.dat" copy.dat
    run -0 grep -c 'kXR_bind successful' "$XRD_LOGFILE"
    assert_output 3
}

@test "download with --streams 3 keeps the content" {
    xrdcp --streams 3 "$SERVER_URL//ref.dat" copy.dat
    run -0 cmp "$REF" copy.dat
}

@test "upload with --streams 3 keeps the content" {
    xrdcp --streams 3 "$REF" "$SERVER_URL/$TARGET"
    run -0 cmp "$REF" "$SERVER_DATA$TARGET"
}

@test "download with 4 substreams for each channel keeps the content" {
    XRD_SUBSTREAMSPERCHANNEL=4 xrdcp "$SERVER_URL//ref.dat" copy.dat
    run -0 cmp "$REF" copy.dat
}

@test "download with 16 chunks in flight keeps the content" {
    XRD_CPPARALLELCHUNKS=16 XRD_CPCHUNKSIZE=65536 xrdcp "$SERVER_URL//ref.dat" copy.dat
    run -0 cmp "$REF" copy.dat
}

@test "upload with 16 chunks in flight keeps the content" {
    XRD_CPPARALLELCHUNKS=16 XRD_CPCHUNKSIZE=65536 xrdcp "$REF" "$SERVER_URL/$TARGET"
    run -0 cmp "$REF" "$SERVER_DATA$TARGET"
}

@test "--parallel 3 uploads all files" {
    mkdir src
    for i in 1 2 3; do make_random_file "src/$i.dat" 100000; done
    xrdfs "$SERVER_URL" mkdir "/p${BATS_TEST_NUMBER}"
    xrdcp --parallel 3 src/1.dat src/2.dat src/3.dat "$SERVER_URL//p${BATS_TEST_NUMBER}/"
    run -0 diff -r src "$SERVER_DATA/p${BATS_TEST_NUMBER}"
}
