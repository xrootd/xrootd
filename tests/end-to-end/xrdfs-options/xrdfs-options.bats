#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

setup() {
    mkdir -p "$BATS_TEST_TMPDIR/xrdfs-options/data/subdirectory"
    printf 'first' > "$BATS_TEST_TMPDIR/xrdfs-options/data/first.txt"
    printf 'nested' \
        > "$BATS_TEST_TMPDIR/xrdfs-options/data/subdirectory/nested.txt"
    printf 'dash' > "$BATS_TEST_TMPDIR/xrdfs-options/-dash.txt"
    printf 'grouped-option' > "$BATS_TEST_TMPDIR/xrdfs-options/-lH"
    printf 'bytes-option' > "$BATS_TEST_TMPDIR/xrdfs-options/-b"
    printf '\000\001\177\200\377' \
        > "$BATS_TEST_TMPDIR/xrdfs-options/data/binary.dat"

    launch_xrootd xrdfs-options.cfg xrdfs-options

    export TEST_ENDPOINT=root://localhost:12965

    local ready=false
    for _ in {1..50}; do
        if "$XRDFS" "$TEST_ENDPOINT" stat /data/first.txt \
            >/dev/null 2>&1; then
            ready=true
            break
        fi
        sleep 0.1
    done
    "$ready"
}

teardown() {
    kill_pid_files 2>/dev/null || true
}

bats::on_failure() {
    print_log_files
}

@test "ls accepts grouped short and gfal-compatible options" {
    run "$XRDFS" "$TEST_ENDPOINT" ls -lH /data/first.txt
    assert_success
    assert_output --partial '/data/first.txt'
    assert_output --partial '5'
}

@test "ls accepts long gfal-compatible options" {
    run "$XRDFS" "$TEST_ENDPOINT" ls \
        --long --human-readable --directory /data
    assert_success
    assert_output --partial '/data'
    refute_output --partial 'first.txt'
}

@test "ls preserves legacy unknown dash-prefixed paths" {
    run bash -c \
        'printf "cd /\nls -dash.txt\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    assert_output --partial '/-dash.txt'
}

@test "ls delimiter addresses a path matching grouped options" {
    run bash -c \
        'printf "cd /\nls -- -lH\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    assert_output --partial '/-lH'
}

@test "ls delimiter does not impose the old argument limit" {
    run "$XRDFS" "$TEST_ENDPOINT" ls -l -u -D -h -- /data/first.txt
    assert_success
    assert_output --partial '/data/first.txt'
}

@test "ls retains recursive listing" {
    run "$XRDFS" "$TEST_ENDPOINT" ls -R /data
    assert_success
    assert_output --partial '/data/subdirectory/nested.txt'
}

@test "cat bytes option preserves binary output" {
    local destination="$BATS_TEST_TMPDIR/binary-download.dat"
    run bash -c '"$1" "$2" cat -b /data/binary.dat > "$3"' \
        _ "$XRDFS" "$TEST_ENDPOINT" "$destination"
    assert_success

    run cmp "$BATS_TEST_TMPDIR/xrdfs-options/data/binary.dat" "$destination"
    assert_success
}

@test "cat long bytes option is accepted" {
    run "$XRDFS" "$TEST_ENDPOINT" cat --bytes /data/first.txt
    assert_success
    assert_output first
}

@test "cat delimiter addresses a file named like an option" {
    run bash -c \
        'printf "cd /\ncat -- -b\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    assert_output --partial 'bytes-option'
}

@test "cat retains output-to-file behavior" {
    local destination="$BATS_TEST_TMPDIR/cat-output.dat"
    run "$XRDFS" "$TEST_ENDPOINT" cat -o "$destination" /data/first.txt
    assert_success

    run cmp "$BATS_TEST_TMPDIR/xrdfs-options/data/first.txt" "$destination"
    assert_success
}

@test "cat bytes option still requires a source" {
    run "$XRDFS" "$TEST_ENDPOINT" cat -b
    assert_failure
    assert_output --partial 'Invalid arguments'
}
