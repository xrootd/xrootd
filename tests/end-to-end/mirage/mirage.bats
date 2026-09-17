#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

readonly MIRAGE=localhost:11969

setup() {
    sleep 0.5
    PORT=${MIRAGE##*:} launch_xrootd mirage.cfg mirage
    sleep 0.5

    run bats_pipe -0 printf 'seed' \| xrdcp - "root://$MIRAGE//file"
}

teardown() {
    kill_pid_files
}

@test "uploaded file reports the uploaded size" {
    run -0 xrdfs "root://$MIRAGE" stat //file
    assert_output --partial 'Size:   4'
}

@test "truncate changes the reported size" {
    run -0 xrdfs "root://$MIRAGE" truncate //file 1000

    run -0 xrdfs "root://$MIRAGE" stat //file
    assert_output --partial 'Size:   1000'
}

@test "empty pattern returns content of the correct size" {
    run -0 xrdfs "root://$MIRAGE" truncate //file 1000
    run -0 xrdfs "root://$MIRAGE" xattr //file del pattern

    # Empty pattern content is arbitrary memory by design (see
    # src/XrdOssMirage/README.md), so only the size can be checked.
    run bats_pipe -0 xrdcp -f "root://$MIRAGE//file" - \| wc -c
    assert_output '1000'
}

@test "single character pattern repeats exactly" {
    run -0 xrdfs "root://$MIRAGE" truncate //file 1000
    run -0 xrdfs "root://$MIRAGE" xattr //file set pattern=a

    run -0 xrdcp -f "root://$MIRAGE//file" -
    assert_output "$(printf 'a%.0s' {1..1000})"
}

@test "string pattern repeats exactly" {
    run -0 xrdfs "root://$MIRAGE" truncate //file 1000
    run -0 xrdfs "root://$MIRAGE" xattr //file set pattern=abcde

    run -0 xrdcp -f "root://$MIRAGE//file" -
    assert_output "$(printf 'abcde%.0s' {1..200})"
}

@test "pattern content is accurate under a small XRD_CPCHUNKSIZE" {
    # The size (10007) and the chunk size (37) are not multiples of the
    # 5-byte pattern, so every chunk starts at a different offset into
    # the pattern. This exercises the wraparound copy out of the
    # once-initialized pattern buffer in XrdOssMirageFile::Read.
    run -0 xrdfs "root://$MIRAGE" truncate //file 10007
    run -0 xrdfs "root://$MIRAGE" xattr //file set pattern=abcde

    XRD_CPCHUNKSIZE=37 run -0 xrdcp -f "root://$MIRAGE//file" -
    assert_output "$(printf 'abcde%.0s' {1..2001})ab"
}

@test "pattern content is accurate under concurrent chunk reads" {
    # XRD_CPPARALLELCHUNKS makes xrdcp issue several Read() calls in
    # flight at once on the same open file, which is exactly the
    # concurrency that the pattern buffer's std::call_once protects
    # against.
    run -0 xrdfs "root://$MIRAGE" truncate //file 10007
    run -0 xrdfs "root://$MIRAGE" xattr //file set pattern=abcde

    XRD_CPCHUNKSIZE=37 XRD_CPPARALLELCHUNKS=8 run -0 xrdcp -f "root://$MIRAGE//file" -
    assert_output "$(printf 'abcde%.0s' {1..2001})ab"
}

@test "pattern content is accurate under concurrent chunk reads from two clients with different chunk sizes" {
    run -0 xrdfs "root://$MIRAGE" truncate //file 10007
    run -0 xrdfs "root://$MIRAGE" xattr //file set pattern=abcde

    XRD_CPCHUNKSIZE=37 xrdcp -f "root://$MIRAGE//file" "$BATS_TEST_TMPDIR/first" &
    local first_pid=$!
    XRD_CPCHUNKSIZE=53 xrdcp -f "root://$MIRAGE//file" "$BATS_TEST_TMPDIR/second" &
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
    run -0 xrdfs "root://$MIRAGE" truncate //file 1073741824
    run -0 xrdfs "root://$MIRAGE" xattr //file set pattern=abcde

    xrdcp -f "root://$MIRAGE//file" /dev/null &
    local first_pid=$!
    xrdcp -f "root://$MIRAGE//file" /dev/null &
    local second_pid=$!

    wait "$first_pid"
    local first_status=$?
    wait "$second_pid"
    local second_status=$?

    assert_equal "$first_status" 0
    assert_equal "$second_status" 0
}

@test "two clients downloading the same file with different chunk sizes both succeed" {
    run -0 xrdfs "root://$MIRAGE" truncate //file 1073741824
    run -0 xrdfs "root://$MIRAGE" xattr //file set pattern=abcde

    XRD_CPCHUNKSIZE=8381608 xrdcp -f "root://$MIRAGE//file" /dev/null &
    local first_pid=$!
    XRD_CPCHUNKSIZE=8388608 xrdcp -f "root://$MIRAGE//file" /dev/null &
    local second_pid=$!

    wait "$first_pid"
    local first_status=$?
    wait "$second_pid"
    local second_status=$?

    assert_equal "$first_status" 0
    assert_equal "$second_status" 0
}
