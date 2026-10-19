#!/usr/bin/env bash

# HTTP status codes of failures. The fault injection of libXrdOssTests fails
# by the name of the file: reads of fail_read.txt after offset 0, the
# creation of no_inode.txt and out_of_inode_quota.txt, and writes after 2 MiB
# to out_of_space_quota.txt. The tests share one server.

load ../setup/server.bash

# BATS stops a test and its processes after this time, thus a client which
# hangs fails its test
# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=120

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    start_server server server http server.cfg
    mkdir "$SERVER_DATA/readonly" "$SERVER_DATA/dir"
    printf "%1048576s" " " | sed 's/ /blah/g' > "$SERVER_DATA/fail_read.txt"
    # bigger than 2 MiB, after which the fault injection fails writes
    make_random_file "$BATS_FILE_TMPDIR/5mb.dat" $(( 5 * 1024 * 1024 ))
    cp "$BATS_FILE_TMPDIR/5mb.dat" "$SERVER_DATA/5mb.dat"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    N=$BATS_TEST_NUMBER
}

# Print the HTTP status of a request with the given curl arguments
status() {
    curl --silent -L -o body.txt --write-out '%{http_code}' "$@"
}

@test "a request with an invalid xrdhttptk fails with 400" {
    run -0 status "$SERVER_HTTP/foo?xrdhttptk=badtoken&xrdhttptime=$(date +%s)"
    assert_output 400
}

# The server closes the connection without a response, but a malformed
# header line gets a 400
@test "a request line without the HTTP version does not get the file" {
    run -0 bash -c "printf 'GET /5mb.dat\r\nHost: localhost\r\n\r\n' |
        python3 '$BATS_TEST_DIRNAME/raw_tcp_status.py' localhost '$SERVER_PORT'"
    refute_output --regexp '^HTTP/1\.[01] 2'
}

@test "the server answers after a request line without the HTTP version" {
    printf 'GET /5mb.dat\r\nHost: localhost\r\n\r\n' |
        python3 "$BATS_TEST_DIRNAME/raw_tcp_status.py" localhost "$SERVER_PORT" || true
    run -0 server_is_up "$SERVER_URL"
}

@test "an upload to a read-only export fails with 403" {
    run -0 status --upload-file "$BATS_FILE_TMPDIR/5mb.dat" "$SERVER_HTTP/readonly/file$N"
    assert_output 403
}

@test "an upload to the path of a directory fails with 409" {
    run -0 status --upload-file "$BATS_FILE_TMPDIR/5mb.dat" "$SERVER_HTTP/dir"
    assert_output 409
}

@test "a GET of a file which does not exist fails with 404" {
    run -0 status "$SERVER_HTTP/dir/file_does_not_exist"
    assert_output 404
}

@test "an upload without free inodes fails with 507" {
    run -0 status --upload-file "$BATS_FILE_TMPDIR/5mb.dat" "$SERVER_HTTP/dir/$N/no_inode.txt"
    assert_output 507
}

@test "an upload beyond the inode quota fails with 507" {
    run -0 status --upload-file "$BATS_FILE_TMPDIR/5mb.dat" "$SERVER_HTTP/dir/$N/out_of_inode_quota.txt"
    assert_output 507
}

@test "a GET with trailers of a file which fails to read starts with 200" {
    run -0 status --raw -H 'TE: trailers' -H 'X-Transfer-Status: true' "$SERVER_HTTP/fail_read.txt"
    assert_output 200
}

@test "a GET with trailers of a file which fails to read reports 500 in the trailer" {
    status --raw -H 'TE: trailers' -H 'X-Transfer-Status: true' "$SERVER_HTTP/fail_read.txt" || true
    run -0 grep -E '^X-Transfer-Status: 500' <(tr -d '\r' < body.txt)
}

@test "HEAD with Want-Digest of a file of 5 MiB succeeds" {
    run -0 status -I -H 'Want-Digest: crc32c' "$SERVER_HTTP/5mb.dat"
    assert_output 200
}

# The tests below check that XrdClHttp neither hangs nor crashes on failures
# of a transfer of several chunks.

@test "an upload with XrdClHttp beyond the space quota fails" {
    make_random_file local.dat $(( 50 * 1024 * 1024 ))
    run ! xrdcp local.dat "$SERVER_HTTP/dir/$N/out_of_space_quota.txt"
    # a crash exits with 128 + the number of the signal
    assert [ "$status" -lt 128 ]
}

@test "an upload with XrdClHttp of several chunks succeeds" {
    make_random_file local.dat $(( 50 * 1024 * 1024 ))
    run -0 xrdcp local.dat "$SERVER_HTTP/dir/$N/upload.dat"
}

@test "a download with XrdClHttp of a file which fails to read fails" {
    run ! xrdcp "$SERVER_HTTP/fail_read.txt" copy.txt
    # a crash exits with 128 + the number of the signal
    assert [ "$status" -lt 128 ]
}

@test "a download with XrdClHttp of a file of 5 MiB keeps the content" {
    xrdcp "$SERVER_HTTP/5mb.dat" copy.dat
    run -0 cmp "$BATS_FILE_TMPDIR/5mb.dat" copy.dat
}
