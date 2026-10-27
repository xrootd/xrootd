#!/usr/bin/env bash

# The purge of the cache, which removes the files which were read first when
# the files use more than 4 MiB. The cache reads 8 files of 1 MiB, in two
# groups 3 s apart, then the tests wait for the purge, which runs each 60 s.
# The tests share this state.

load cache.bash

# cached_bytes: the bytes of all data files in the cache
cached_bytes() {
    cat "$CACHE_DATA"/purge-*.dat 2>/dev/null | wc -c
}

_purged() {
    (( $(cached_bytes) <= 4 * 1024 * 1024 ))
}

setup_file() {
    start_cache purge.cfg
    for i in {1..8}; do
        make_origin_file "purge-$i.dat" 8
    done
    for i in {1..4}; do
        xrdcp -f "$CACHE_URL//purge-$i.dat" /dev/null
    done
    sleep 3
    for i in {5..8}; do
        xrdcp -f "$CACHE_URL//purge-$i.dat" /dev/null
    done
    wait_for 200 _purged
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
}

@test "the purge brings the files of the cache below 4 MiB" {
    run -0 cached_bytes
    assert [ "$output" -le $((4 * 1024 * 1024)) ]
}

@test "the purge removes the files which were read first" {
    run ! compgen -G "$CACHE_DATA/purge-[1-4].dat"
}

@test "the purge removes the cinfo files of the files which it removes" {
    run ! compgen -G "$CACHE_DATA/purge-[1-4].dat.cinfo"
}

@test "the purge keeps a file which was read last" {
    run -0 compgen -G "$CACHE_DATA/purge-[5-8].dat"
}

@test "a read of a purged file fetches it into the cache again" {
    xrdcp -f "$CACHE_URL//purge-1.dat" out.dat
    wait_for_accesses purge-1.dat 1
    run -0 cinfo_blocks purge-1.dat
    assert_output '8 8 complete'
}
