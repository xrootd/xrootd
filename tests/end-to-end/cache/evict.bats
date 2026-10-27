#!/usr/bin/env bash

# Opens with only-if-cached, which do not read from the origin, and the evict
# command, which removes a file from the cache. The tests share the origin,
# the cache and the collector; each test has its own file of 4 MiB.

load cache.bash

setup_file() {
    start_cache onlyifcached.cfg
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    F="t${BATS_TEST_NUMBER}.dat"
    make_origin_file "$F" 32
}

# Read the whole file into the cache and wait until it is complete. The cache
# writes the cinfo file in its final sync, and closes the file only after the
# sync; until then an evict fails with "file is in use". The file_close record
# comes after the close.
fill() {
    xrdcp -f "$CACHE_URL//$F" fill.dat
    wait_for_complete "$F"
    wait_for_close "$F" 1
}

evict() {
    xrdfs "$CACHE_URL" cache evict "/$F"
}

@test "only-if-cached refuses a file which is not in the cache" {
    run ! xrdcp -f "$CACHE_URL//$F?only-if-cached" out.dat
}

@test "a refused only-if-cached open makes no cinfo file" {
    xrdcp -f "$CACHE_URL//$F?only-if-cached" out.dat || true
    run ! test -e "$(cinfo "$F")"
}

@test "only-if-cached refuses a file which is only partly in the cache" {
    xrdpfc-read read "$CACHE_URL//$F" part.dat 0:4096
    wait_for_accesses "$F" 1
    run ! xrdcp -f "$CACHE_URL//$F?only-if-cached" out.dat
}

@test "only-if-cached reads a file which is complete in the cache" {
    fill
    xrdcp -f "$CACHE_URL//$F?only-if-cached" out.dat
    run -0 cmp "$ORIGIN_DATA/$F" out.dat
}

@test "an only-if-cached read fetches nothing from the origin" {
    fill
    xrdcp -f "$CACHE_URL//$F?only-if-cached" out.dat
    run -0 fetched_total "$F"
    assert_output $((32 * BLOCK))
}

@test "evict removes the data file from the cache" {
    fill
    evict
    run -0 wait_for 10 test ! -e "$CACHE_DATA/$F"
}

@test "evict removes the cinfo file from the cache" {
    fill
    evict
    run -0 wait_for 10 test ! -e "$(cinfo "$F")"
}

@test "evict keeps the file at the origin" {
    fill
    evict
    wait_for 10 test ! -e "$(cinfo "$F")"
    xrdcp -f "$ORIGIN_URL//$F" out.dat
    run -0 cmp "$ORIGIN_DATA/$F" out.dat
}

@test "only-if-cached refuses a file after its eviction" {
    fill
    evict
    wait_for 10 test ! -e "$(cinfo "$F")"
    run ! xrdcp -f "$CACHE_URL//$F?only-if-cached" out.dat
}

@test "a read after an eviction fetches the whole file again" {
    fill
    evict
    wait_for 10 test ! -e "$(cinfo "$F")"
    xrdcp -f "$CACHE_URL//$F" out.dat
    run -0 fetched_total "$F"
    assert_output $((2 * 32 * BLOCK))
}
