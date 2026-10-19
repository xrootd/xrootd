#!/usr/bin/env bash

# A restart of the cache keeps the files on disk. Before the restart, the
# cache has one complete file and one file with only blocks 0 and 8 of 32.
# The tests share this state. The last test restarts the origin, under the
# open connection of the cache.

load cache.bash

setup_file() {
    start_cache
    make_origin_file full.dat 32
    make_origin_file part.dat 32
    xrdcp -f "$CACHE_URL//full.dat" "$BATS_FILE_TMPDIR/full.dat"
    xrdpfc-read read "$CACHE_URL//part.dat" "$BATS_FILE_TMPDIR/part.dat" 0:4096 $((8 * BLOCK)):4096
    wait_for_close full.dat 1
    wait_for_close part.dat 1
    restart_server cache
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
}

@test "a complete file is complete after a restart of the cache" {
    run -0 cinfo_blocks full.dat
    assert_output '32 32 complete'
}

@test "a partial file keeps its blocks after a restart of the cache" {
    run -0 cinfo_block_map part.dat
    assert_output 'x.......x.......................'
}

@test "a read of a complete file after a restart fetches nothing" {
    xrdcp -f "$CACHE_URL//full.dat" out.dat
    run -0 close_field full.dat 2 b_todisk
    assert_output 0
}

@test "a read of the cached blocks of a partial file after a restart fetches nothing" {
    xrdpfc-read read "$CACHE_URL//part.dat" out.dat 0:4096 $((8 * BLOCK)):4096
    run -0 close_field part.dat 2 b_todisk
    assert_output 0
}

@test "a full read of a partial file after a restart fetches only the missing blocks" {
    xrdcp -f "$CACHE_URL//part.dat" out.dat
    # the blocks 0 and 8 came before the restart, the 30 others now
    run -0 fetched_total part.dat
    assert_output $((32 * BLOCK))
}

@test "a full read of a partial file after a restart has the content of the origin" {
    xrdcp -f "$CACHE_URL//part.dat" out.dat
    run -0 cmp "$ORIGIN_DATA/part.dat" out.dat
}

@test "a partial file is complete after a full read after a restart" {
    xrdcp -f "$CACHE_URL//part.dat" out.dat
    wait_for_complete part.dat
    run -0 cinfo_blocks part.dat
    assert_output '32 32 complete'
}

@test "a read through the cache after a restart of the origin fetches the file" {
    make_origin_file after.dat 8
    xrdcp -f "$CACHE_URL//part.dat" /dev/null
    restart_server origin
    xrdcp -f "$CACHE_URL//after.dat" out.dat
    run -0 cmp "$ORIGIN_DATA/after.dat" out.dat
}
