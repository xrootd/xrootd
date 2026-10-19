#!/usr/bin/env bash

# A cache which checks the page checksums of the files on its disk: a file
# which changes on the disk of the cache fails to read. The cache is also a
# forwarding proxy. The tests share the
# origin, the cache and the collector; each test has its own file.

load cache.bash

setup_file() {
    start_cache checksum.cfg
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    F="t${BATS_TEST_NUMBER}.txt"
    for i in {1..2000}; do echo "line $i of the file"; done > "$ORIGIN_DATA/$F"
}

# Read the file into the cache, then change its first byte on the disk of
# the cache
corrupt() {
    xrdcp -f "$CACHE_URL//$F" first.txt
    wait_for_close "$F" 1
    printf 'X' | dd of="$CACHE_DATA/$F" bs=1 count=1 conv=notrunc status=none
}

@test "a read through a cache which checks checksums has the content of the origin" {
    xrdcp -f "$CACHE_URL//$F" out.txt
    run -0 cmp "$ORIGIN_DATA/$F" out.txt
}

@test "a second read through a cache which checks checksums has the content of the origin" {
    xrdcp -f "$CACHE_URL//$F" first.txt
    wait_for_close "$F" 1
    xrdcp -f "$CACHE_URL//$F" second.txt
    run -0 cmp "$ORIGIN_DATA/$F" second.txt
}

@test "a read of a file which changed on the disk of the cache fails with status 54" {
    corrupt
    run -54 xrdcp -f "$CACHE_URL//$F" out.txt
}

@test "a read of a file which changed on the disk of the cache reports a checksum error" {
    corrupt
    run ! xrdcp -f "$CACHE_URL//$F" out.txt
    assert_output --partial 'Server responded with an error: [3019]'
}

@test "a forwarding cache reads a file by the URL of the origin in the path" {
    xrdcp -f "$CACHE_URL//root://localhost:$ORIGIN_PORT//$F" out.txt
    run -0 cmp "$ORIGIN_DATA/$F" out.txt
}
