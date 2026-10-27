#!/usr/bin/env bash

# The prefetch of the cache, with up to 8 blocks: a read fetches more blocks
# than it reads, but never more than the file. The tests share the origin,
# the cache and the collector; each test has its own file. read.bats checks
# the same reads with prefetch off.

load cache.bash

setup_file() {
    start_cache prefetch.cfg
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    F="t${BATS_TEST_NUMBER}.dat"
    make_origin_file "$F" 32
}

@test "with prefetch a read of 4 KiB returns the data of the range" {
    xrdpfc-read read "$CACHE_URL//$F" out.dat 0:4096
    origin_range 0:4096 "$F" > expected.dat
    run -0 cmp expected.dat out.dat
}

@test "with prefetch a read of 4 KiB fetches more than one block" {
    xrdpfc-read read "$CACHE_URL//$F" out.dat 0:4096
    run -0 close_field "$F" 1 b_todisk
    assert [ "$output" -gt "$BLOCK" ]
}

@test "with prefetch a read of 4 KiB prefetches bytes" {
    xrdpfc-read read "$CACHE_URL//$F" out.dat 0:4096
    run -0 close_field "$F" 1 b_prefetch
    assert [ "$output" -gt 0 ]
}

@test "the prefetched bytes are part of the fetched bytes" {
    xrdpfc-read read "$CACHE_URL//$F" out.dat 0:4096
    assert [ "$(close_field "$F" 1 b_prefetch)" -le "$(close_field "$F" 1 b_todisk)" ]
}

@test "with prefetch a full read fetches the file once" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    run -0 close_field "$F" 1 b_todisk
    assert_output $((32 * BLOCK))
}

@test "with prefetch a full read leaves the file complete in the cache" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    wait_for_accesses "$F" 1
    run -0 cinfo_blocks "$F"
    assert_output '32 32 complete'
}

@test "a second read of a file which prefetch completed fetches nothing" {
    xrdcp -f "$CACHE_URL//$F" first.dat
    wait_for_close "$F" 1
    xrdcp -f "$CACHE_URL//$F" second.dat
    run -0 close_field "$F" 2 b_todisk
    assert_output 0
}
