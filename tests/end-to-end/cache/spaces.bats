#!/usr/bin/env bash

# A cache with the data files and the .cinfo files in two separate spaces.
# The data directory of the cache then has links into the spaces. The tests
# share the origin, the cache and the collector; each test has its own file
# of 2 MiB.

load cache.bash

setup_file() {
    export SPACES="$BATS_FILE_TMPDIR/spaces"
    mkdir -p "$SPACES/space-data" "$SPACES/space-meta"
    SPACES=$(cd "$SPACES" && pwd -P)
    start_cache spaces.cfg
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    F="t${BATS_TEST_NUMBER}.dat"
    make_origin_file "$F" 16
}

# Read the whole file into the cache and wait for its access record
fill() {
    xrdcp -f "$CACHE_URL//$F" fill.dat
    wait_for_accesses "$F" 1
}

@test "a read through a cache with spaces has the content of the origin" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    run -0 cmp "$ORIGIN_DATA/$F" out.dat
}

@test "a cold read through a cache with spaces fetches the whole file" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    run -0 close_field "$F" 1 b_todisk
    assert_output $((16 * BLOCK))
}

@test "the data file in the cache is a link into the data space" {
    fill
    run -0 readlink -f "$CACHE_DATA/$F"
    assert_output --regexp "^$SPACES/space-data/data/"
}

@test "the cinfo file in the cache is a link into the meta space" {
    fill
    run -0 readlink -f "$(cinfo "$F")"
    assert_output --regexp "^$SPACES/space-meta/meta/"
}

@test "the data file in the data space has the content of the origin" {
    fill
    run -0 cmp "$ORIGIN_DATA/$F" "$(readlink "$CACHE_DATA/$F")"
}

@test "the cinfo file in the meta space shows the file complete" {
    fill
    run -0 cinfo_blocks "$F"
    assert_output '16 16 complete'
}

@test "the meta space has no data file" {
    fill
    run -0 find "$SPACES/space-meta" -type f -size "$((16 * BLOCK))c"
    assert_output ''
}

@test "a warm read through a cache with spaces fetches nothing" {
    xrdcp -f "$CACHE_URL//$F" cold.dat
    wait_for_close "$F" 1
    xrdcp -f "$CACHE_URL//$F" warm.dat
    run -0 close_field "$F" 2 b_todisk
    assert_output 0
}
