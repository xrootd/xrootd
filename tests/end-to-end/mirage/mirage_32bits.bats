#!/usr/bin/env bash

load ../setup/server.bash

# One byte above 5 GiB. The value does not fit in 32 bits.
readonly LARGE_SIZE=5368709121

# Each test has its own file, with the content "seed"
setup() {
    start_server mirage.cfg

    FILE="/t${BATS_TEST_NUMBER}"
    run -0 bats_pipe printf 'seed' \| xrdcp - "$SERVER_URL/$FILE"
}

teardown() {
    stop_servers
}

@test "uploading a file larger than 5 GB reports the uploaded size" {
    run -0 bats_pipe head -c "$LARGE_SIZE" /dev/zero \| xrdcp - "$SERVER_URL/${FILE}_large"

    run -0 xrdfs "$SERVER_URL" stat "${FILE}_large"
    assert_output --regexp "Size:   ${LARGE_SIZE}[^0-9]"
}

@test "truncating a file to more than 5 GB reports the new size" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" "$LARGE_SIZE"

    run -0 xrdfs "$SERVER_URL" stat "$FILE"
    assert_output --regexp "Size:   ${LARGE_SIZE}[^0-9]"
}

@test "truncating a file larger than 5 GB to a smaller size reports the smaller size" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" "$LARGE_SIZE"
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" 1000

    run -0 xrdfs "$SERVER_URL" stat "$FILE"
    assert_output --regexp 'Size:   1000[^0-9]'
}

@test "downloading a file larger than 5 GB succeeds" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" "$LARGE_SIZE"

    run -0 xrdcp -f "$SERVER_URL/$FILE" /dev/null
}

@test "downloading a file larger than 5 GB returns the full number of bytes" {
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" "$LARGE_SIZE"
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" set pattern=abcde

    run -0 bats_pipe xrdcp -f "$SERVER_URL/$FILE" - \| wc -c
    assert_output --regexp "^ *${LARGE_SIZE} *\$"
}

@test "uploading a file larger than 5 GB and downloading it again preserves the size" {
    run -0 bats_pipe head -c "$LARGE_SIZE" /dev/zero \| xrdcp - "$SERVER_URL/${FILE}_large"

    run -0 bats_pipe xrdcp -f "$SERVER_URL/${FILE}_large" - \| wc -c
    assert_output --regexp "^ *${LARGE_SIZE} *\$"
}

@test "downloading the end of a file larger than 5 GB returns the pattern at the correct offset" {
    # 5368709121 leaves a remainder of 1 modulo 5, so the last 10 bytes
    # start at pattern index 1 only if the full 64-bit offset is used.
    run -0 xrdfs "$SERVER_URL" truncate "$FILE" "$LARGE_SIZE"
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" set pattern=abcde

    run -0 bats_pipe xrdcp -f "$SERVER_URL/$FILE" - \| tail -c 10
    assert_output "bcdeabcdea"
}
