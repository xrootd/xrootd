#!/usr/bin/env bash

bats_require_minimum_version 1.10.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

readonly MIRAGE="localhost:11970"

# One byte above 5 GiB. The value does not fit in 32 bits.
readonly LARGE_SIZE=5368709121

setup() {
    sleep 0.5
    PORT=${MIRAGE##*:} launch_xrootd mirage.cfg mirage
    sleep 0.5

    run -0 bats_pipe printf 'seed' \| xrdcp -f - "root://$MIRAGE//file"
}

teardown() {
    kill_pid_files
}

@test "uploading a file larger than 5 GB reports the uploaded size" {
    run -0 bats_pipe head -c "$LARGE_SIZE" /dev/zero \| xrdcp - "root://$MIRAGE//large"

    run -0 xrdfs "root://$MIRAGE" stat //large
    assert_output --regexp "Size:   ${LARGE_SIZE}[^0-9]"
}

@test "truncating a file to more than 5 GB reports the new size" {
    run -0 xrdfs "root://$MIRAGE" truncate //file "$LARGE_SIZE"

    run -0 xrdfs "root://$MIRAGE" stat //file
    assert_output --regexp "Size:   ${LARGE_SIZE}[^0-9]"
}

@test "truncating a file larger than 5 GB to a smaller size reports the smaller size" {
    run -0 xrdfs "root://$MIRAGE" truncate //file "$LARGE_SIZE"
    run -0 xrdfs "root://$MIRAGE" truncate //file 1000

    run -0 xrdfs "root://$MIRAGE" stat //file
    assert_output --regexp 'Size:   1000[^0-9]'
}

@test "downloading a file larger than 5 GB succeeds" {
    run -0 xrdfs "root://$MIRAGE" truncate //file "$LARGE_SIZE"

    run -0 xrdcp -f "root://$MIRAGE//file" /dev/null
}

@test "downloading a file larger than 5 GB returns the full number of bytes" {
    run -0 xrdfs "root://$MIRAGE" truncate //file "$LARGE_SIZE"
    run -0 xrdfs "root://$MIRAGE" xattr //file set pattern=abcde

    run bats_pipe -0 xrdcp -f "root://$MIRAGE//file" - \| wc -c
    assert_output --regexp "^ *${LARGE_SIZE} *\$"
}

@test "uploading a file larger than 5 GB and downloading it again preserves the size" {
    run -0 bats_pipe head -c "$LARGE_SIZE" /dev/zero \| xrdcp - "root://$MIRAGE//large"

    run bats_pipe -0 xrdcp -f "root://$MIRAGE//large" - \| wc -c
    assert_output --regexp "^ *${LARGE_SIZE} *\$"
}

@test "downloading the end of a file larger than 5 GB returns the pattern at the correct offset" {
    # 5368709121 leaves a remainder of 1 modulo 5, so the last 10 bytes
    # start at pattern index 1 only if the full 64-bit offset is used.
    run -0 xrdfs "root://$MIRAGE" truncate //file "$LARGE_SIZE"
    run -0 xrdfs "root://$MIRAGE" xattr //file set pattern=abcde

    run bats_pipe -0 xrdcp -f "root://$MIRAGE//file" - \| tail -c 10
    assert_output "bcdeabcdea"
}
