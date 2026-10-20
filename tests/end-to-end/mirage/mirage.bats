#!/usr/bin/env bash

load ../setup/server.bash

setup_file() {
    start_server server server mirage.cfg
}

teardown_file() {
    stop_servers
}

# Each test has its own file, with the content "seed"
setup() {
    FILE="/t${BATS_TEST_NUMBER}"
    run -0 bats_pipe printf 'seed' \| xrdcp - "$SERVER_URL/$FILE"
}

@test "uploaded file reports the uploaded size" {
    run -0 xrdfs "$SERVER_URL" stat "$FILE"
    assert_output --regexp 'Size:   4[^0-9]'
}

@test "truncate changes the reported size" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" 1000

    run -0 xrdfs "$SERVER_URL" stat "$FILE"
    assert_output --regexp 'Size:   1000[^0-9]'
}

@test "empty pattern returns content of the correct size" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" 1000
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" del pattern

    # Empty pattern content is arbitrary memory by design (see
    # src/XrdOssMirage/README.md), so only the size can be checked.
    run bats_pipe -0 xrdcp -f "$SERVER_URL/$FILE" - \| wc -c
    assert_output --regexp '^ *1000 *$'
}

@test "single character pattern repeats exactly" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" 1000
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" set pattern=a

    run -0 xrdcp -f "$SERVER_URL/$FILE" -
    assert_output "$(printf 'a%.0s' {1..1000})"
}

@test "string pattern repeats exactly" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" 1000
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" set pattern=abcde

    run -0 xrdcp -f "$SERVER_URL/$FILE" -
    assert_output "$(printf 'abcde%.0s' {1..200})"
}

@test "pattern content is accurate under a small XRD_CPCHUNKSIZE" {
    # The size (10007) and the chunk size (37) are not multiples of the
    # 5-byte pattern, so every chunk starts at a different offset into
    # the pattern. This exercises the wraparound copy out of the
    # once-initialized pattern buffer in XrdOssMirageFile::Read.
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" 10007
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" set pattern=abcde

    XRD_CPCHUNKSIZE=37 run -0 xrdcp -f "$SERVER_URL/$FILE" -
    assert_output "$(printf 'abcde%.0s' {1..2001})ab"
}

@test "pattern content is accurate under concurrent chunk reads" {
    # XRD_CPPARALLELCHUNKS makes xrdcp issue several Read() calls in
    # flight at once on the same open file, which is exactly the
    # concurrency that the pattern buffer's std::call_once protects
    # against.
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" 10007
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" set pattern=abcde

    XRD_CPCHUNKSIZE=37 XRD_CPPARALLELCHUNKS=8 run -0 xrdcp -f "$SERVER_URL/$FILE" -
    assert_output "$(printf 'abcde%.0s' {1..2001})ab"
}

@test "pattern content is accurate under concurrent chunk reads from two clients with different chunk sizes" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" 10007
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" set pattern=abcde

    XRD_CPCHUNKSIZE=37 xrdcp -f "$SERVER_URL/$FILE" "$BATS_TEST_TMPDIR/first" &
    local first_pid=$!
    XRD_CPCHUNKSIZE=53 xrdcp -f "$SERVER_URL/$FILE" "$BATS_TEST_TMPDIR/second" &
    local second_pid=$!

    wait "$first_pid"
    local first_status=$?
    wait "$second_pid"
    local second_status=$?

    assert_equal "$first_status" 0
    assert_equal "$second_status" 0

    run -0 cat "$BATS_TEST_TMPDIR/first"
    assert_output "$(printf 'abcde%.0s' {1..2001})ab"

    run -0 cat "$BATS_TEST_TMPDIR/second"
    assert_output "$(printf 'abcde%.0s' {1..2001})ab"
}

@test "two clients downloading the same file at the same time both succeed" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" 1073741824
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" set pattern=abcde

    xrdcp -f "$SERVER_URL/$FILE" /dev/null &
    local first_pid=$!
    xrdcp -f "$SERVER_URL/$FILE" /dev/null &
    local second_pid=$!

    wait "$first_pid"
    local first_status=$?
    wait "$second_pid"
    local second_status=$?

    assert_equal "$first_status" 0
    assert_equal "$second_status" 0
}

@test "two clients downloading the same file with different chunk sizes both succeed" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" 1073741824
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" set pattern=abcde

    XRD_CPCHUNKSIZE=8381608 xrdcp -f "$SERVER_URL/$FILE" /dev/null &
    local first_pid=$!
    XRD_CPCHUNKSIZE=8388608 xrdcp -f "$SERVER_URL/$FILE" /dev/null &
    local second_pid=$!

    wait "$first_pid"
    local first_status=$?
    wait "$second_pid"
    local second_status=$?

    assert_equal "$first_status" 0
    assert_equal "$second_status" 0
}

@test "a download returns as many bytes as were uploaded" {
    run -0 bats_pipe xrdcp -f "$SERVER_URL/$FILE" - \| wc -c
    assert_output --regexp '^ *4 *$'
}

@test "the attribute open.return_code makes an open fail with that error" {
    xrdfs "$SERVER_URL" xattr "$FILE" set open.return_code=12
    run ! xrdcp -f "$SERVER_URL/$FILE" -
    # 12 is ENOMEM, which the server returns as kXR_NoMemory (3008); the text
    # of the error depends on the C library
    assert_output --partial '[3008]'
}

@test "the attribute read.return_code makes a read fail with that error" {
    xrdfs "$SERVER_URL" truncate "$FILE" 1000
    xrdfs "$SERVER_URL" xattr "$FILE" set read.return_code=12
    run ! xrdcp -f "$SERVER_URL/$FILE" -
    assert_output --partial '[3008]'
}

# the server reports a failed close only for a file open for writing
@test "the attribute close.return_code makes a close fail with that error" {
    echo data > "$BATS_TEST_TMPDIR/upload"
    xrdfs "$SERVER_URL" xattr "$FILE" set close.return_code=12
    run ! xrdcp -f "$BATS_TEST_TMPDIR/upload" "$SERVER_URL/$FILE"
    assert_output --partial '[3008]'
}

@test "an open succeeds again after the removal of open.return_code" {
    xrdfs "$SERVER_URL" xattr "$FILE" set open.return_code=12
    xrdfs "$SERVER_URL" xattr "$FILE" del open.return_code
    run -0 xrdcp -f "$SERVER_URL/$FILE" -
}
