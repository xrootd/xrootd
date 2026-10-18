#!/usr/bin/env bash

# Rate limits, timeouts, retries, sources and persist-on-successful-close of
# xrdcp. The tests share one server, with the files /ref.dat and
# /fail_read.txt on it. The fault injection of libXrdOssTests fails every
# read of fail_read.txt after offset 0.

load ../setup/server.bash

# 16 chunks of 256 KiB, thus a rate of 1 MB/s makes a transfer last 4 s
readonly SIZE=$(( 4 * 1024 * 1024 ))
readonly CHUNK=$(( 256 * 1024 ))

setup_file() {
    start_server server server

    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" "$SIZE"
    cp "$REF" "$SERVER_DATA/ref.dat"
    cp "$REF" "$SERVER_DATA/fail_read.txt"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    TARGET="/t${BATS_TEST_NUMBER}.dat"
}

@test "--xrate above --xrate-threshold completes the upload" {
    run -0 xrdcp --xrate 32M --xrate-threshold 30M "$REF" "$SERVER_URL/$TARGET"
}

@test "XRD_CPTIMEOUT stops an upload which --xrate makes slow" {
    XRD_CPCHUNKSIZE=$CHUNK XRD_CPTIMEOUT=1 run ! xrdcp --xrate 1M "$REF" "$SERVER_URL/$TARGET"
    assert_output --partial 'CPTimeout exceeded'
}

@test "--retry-policy continue completes an upload after a timeout" {
    XRD_CPCHUNKSIZE=$CHUNK XRD_CPTIMEOUT=2 \
        xrdcp --xrate 1M --retry 3 --retry-policy continue "$REF" "$SERVER_URL/$TARGET"
    run -0 cmp "$REF" "$SERVER_DATA$TARGET"
}

@test "--sources 3 downloads the file" {
    xrdcp --sources 3 "$SERVER_URL//ref.dat" copy.dat
    run -0 cmp "$REF" copy.dat
}

@test "download of a file which fails to read fails" {
    XRD_CPCHUNKSIZE=$CHUNK run ! xrdcp "$SERVER_URL//fail_read.txt" copy.dat
    # EIO is kXR_IOError (3007); the text of the error depends on the C library
    assert_output --partial '[3007]'
}

@test "--posc removes the local file of a download which fails" {
    XRD_CPCHUNKSIZE=$CHUNK xrdcp --posc "$SERVER_URL//fail_read.txt" copy.dat || true
    run ! test -e copy.dat
}
