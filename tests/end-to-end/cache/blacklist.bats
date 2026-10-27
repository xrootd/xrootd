#!/usr/bin/env bash

# The decision plugin libXrdBlacklistDecision, which keeps the files below
# /nocache out of the cache. The tests share the origin, the cache and the
# collector; each test has its own files of 1 MiB.

load cache.bash

setup_file() {
    start_cache blacklist.cfg
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    BLOCKED="nocache/t${BATS_TEST_NUMBER}.dat"
    ALLOWED="t${BATS_TEST_NUMBER}.dat"
    make_origin_file "$BLOCKED" 8
    make_origin_file "$ALLOWED" 8
}

@test "a read of a file in the blacklist has the content of the origin" {
    xrdcp -f "$CACHE_URL//$BLOCKED" out.dat
    run -0 cmp "$ORIGIN_DATA/$BLOCKED" out.dat
}

@test "a file in the blacklist is not in the cache after two reads" {
    xrdcp -f "$CACHE_URL//$BLOCKED" first.dat
    xrdcp -f "$CACHE_URL//$BLOCKED" second.dat
    run ! test -e "$CACHE_DATA/$BLOCKED" -o -e "$(cinfo "$BLOCKED")"
}

@test "a read of a file in the blacklist sends no file_close record" {
    xrdcp -f "$CACHE_URL//$BLOCKED" blocked.dat
    # the g-stream sends the records in order, thus the record of a later
    # read shows that a record of the first read would be there
    xrdcp -f "$CACHE_URL//$ALLOWED" allowed.dat
    wait_for_close "$ALLOWED" 1
    run ! close_records "$BLOCKED"
}

@test "a file outside the blacklist is fetched into the cache" {
    xrdcp -f "$CACHE_URL//$ALLOWED" out.dat
    run -0 close_field "$ALLOWED" 1 b_todisk
    assert_output $((8 * BLOCK))
}

@test "a file outside the blacklist is complete in the cache after a read" {
    xrdcp -f "$CACHE_URL//$ALLOWED" out.dat
    wait_for_accesses "$ALLOWED" 1
    run -0 cinfo_blocks "$ALLOWED"
    assert_output '8 8 complete'
}

@test "a second read of a file outside the blacklist fetches nothing" {
    xrdcp -f "$CACHE_URL//$ALLOWED" first.dat
    wait_for_close "$ALLOWED" 1
    xrdcp -f "$CACHE_URL//$ALLOWED" second.dat
    run -0 close_field "$ALLOWED" 2 b_todisk
    assert_output 0
}
