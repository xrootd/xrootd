#!/usr/bin/env bash

# The read path of the cache (XCache), with prefetch off: which blocks a read
# fetches from the origin, and what the cache keeps on disk. The tests share
# the origin, the cache and the collector; each test has its own file.

load cache.bash

setup_file() {
    start_cache debug.cfg
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    F="t${BATS_TEST_NUMBER}.dat"
    make_origin_file "$F" 32
}

@test "a file read through the cache has the content of the origin" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    run -0 cmp "$ORIGIN_DATA/$F" out.dat
}

@test "a file smaller than a block read through the cache has the content of the origin" {
    head -c 1000 /dev/urandom > "$ORIGIN_DATA/$F"
    xrdcp -f "$CACHE_URL//$F" out.dat
    run -0 cmp "$ORIGIN_DATA/$F" out.dat
}

@test "a cold read fetches the whole file from the origin" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    run -0 close_field "$F" 1 b_todisk
    assert_output $((32 * BLOCK))
}

@test "a cold read without prefetch prefetches nothing" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    run -0 close_field "$F" 1 b_prefetch
    assert_output 0
}

@test "a cold read downloads all blocks" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    run -0 close_field "$F" 1 n_blks_done
    assert_output 32
}

@test "a cold read leaves the file complete in the cache" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    wait_for_accesses "$F" 1
    run -0 cinfo_blocks "$F"
    assert_output '32 32 complete'
}

@test "the hits and the misses of a cold read add up to the size of the file" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    wait_for_accesses "$F" 1
    read -r hit miss _ <<< "$(cinfo_access "$F" 1)"
    assert_equal $((hit + miss)) $((32 * BLOCK))
}

@test "a cold read does not bypass the cache" {
    xrdcp -f "$CACHE_URL//$F" out.dat
    wait_for_accesses "$F" 1
    read -r _ _ bypass <<< "$(cinfo_access "$F" 1)"
    assert_equal "$bypass" 0
}

@test "a warm read fetches nothing from the origin" {
    xrdcp -f "$CACHE_URL//$F" cold.dat
    wait_for_close "$F" 1
    xrdcp -f "$CACHE_URL//$F" warm.dat
    run -0 close_field "$F" 2 b_todisk
    assert_output 0
}

@test "a warm read is all hits" {
    xrdcp -f "$CACHE_URL//$F" cold.dat
    wait_for_close "$F" 1
    xrdcp -f "$CACHE_URL//$F" warm.dat
    wait_for_accesses "$F" 2
    run -0 cinfo_access "$F" 2
    assert_output "$((32 * BLOCK)) 0 0"
}

@test "a warm read has the content of the origin" {
    xrdcp -f "$CACHE_URL//$F" cold.dat
    wait_for_close "$F" 1
    xrdcp -f "$CACHE_URL//$F" warm.dat
    run -0 cmp "$ORIGIN_DATA/$F" warm.dat
}

@test "a read of 4 KiB returns the data of the range" {
    xrdpfc-read read "$CACHE_URL//$F" out.dat 0:4096
    origin_range 0:4096 "$F" > expected.dat
    run -0 cmp expected.dat out.dat
}

@test "a read of 4 KiB fetches the one block which holds it" {
    xrdpfc-read read "$CACHE_URL//$F" out.dat 0:4096
    run -0 close_field "$F" 1 b_todisk
    assert_output "$BLOCK"
}

@test "a read of 4 KiB leaves one block of the file in the cache" {
    xrdpfc-read read "$CACHE_URL//$F" out.dat 0:4096
    wait_for_accesses "$F" 1
    run -0 cinfo_blocks "$F"
    assert_output '32 1 incomplete'
}

@test "a second read of a block in the cache fetches nothing" {
    xrdpfc-read read "$CACHE_URL//$F" first.dat 0:4096
    wait_for_close "$F" 1
    xrdpfc-read read "$CACHE_URL//$F" second.dat 0:4096
    run -0 close_field "$F" 2 b_todisk
    assert_output 0
}

@test "a vector read returns the chunks in their order" {
    xrdpfc-read readv "$CACHE_URL//$F" out.dat 0:4096 $((8 * BLOCK)):4096 $((24 * BLOCK)):4096
    { origin_range 0:4096 "$F"; origin_range $((8 * BLOCK)):4096 "$F"; origin_range $((24 * BLOCK)):4096 "$F"; } > expected.dat
    run -0 cmp expected.dat out.dat
}

@test "a vector read fetches only the blocks of its chunks which are not in the cache" {
    xrdpfc-read read "$CACHE_URL//$F" first.dat 0:4096
    wait_for_close "$F" 1
    xrdpfc-read readv "$CACHE_URL//$F" out.dat 0:4096 $((8 * BLOCK)):4096 $((24 * BLOCK)):4096
    run -0 close_field "$F" 2 b_todisk
    assert_output $((2 * BLOCK))
}

@test "a vector read leaves only the blocks of its chunks in the cache" {
    xrdpfc-read readv "$CACHE_URL//$F" out.dat 0:4096 $((8 * BLOCK)):4096 $((24 * BLOCK)):4096
    wait_for_accesses "$F" 1
    run -0 cinfo_block_map "$F"
    assert_output 'x.......x...............x.......'
}

@test "concurrent readers of the same blocks fetch them once" {
    xrdpfc-read multi "$CACHE_URL//$F" out.dat 4 $((2 * BLOCK))
    run -0 fetched_total "$F"
    assert_output $((2 * BLOCK))
}

@test "concurrent readers of the same blocks get the data" {
    xrdpfc-read multi "$CACHE_URL//$F" out.dat 4 $((2 * BLOCK))
    origin_range 0:$((2 * BLOCK)) "$F" > expected.dat
    run -0 cmp expected.dat out.dat
}

@test "stat through the cache of a partial file reports the size at the origin" {
    xrdpfc-read read "$CACHE_URL//$F" out.dat 0:4096
    wait_for_accesses "$F" 1
    run -0 xrdfs "$CACHE_URL" stat "/$F"
    assert_line --regexp "^Size: +$((32 * BLOCK))$"
}

@test "a write through the cache fails" {
    make_random_file upload.dat 4096
    run ! xrdcp -f upload.dat "$CACHE_URL//$F.new"
}

@test "a failed write through the cache leaves no file in the cache" {
    make_random_file upload.dat 4096
    xrdcp -f upload.dat "$CACHE_URL//$F.new" || true
    run ! test -e "$CACHE_DATA/$F.new" -o -e "$(cinfo "$F.new")"
}

@test "a failed write through the cache leaves no file at the origin" {
    make_random_file upload.dat 4096
    xrdcp -f upload.dat "$CACHE_URL//$F.new" || true
    run ! test -e "$ORIGIN_DATA/$F.new"
}

@test "a read through the cache of a file which does not exist fails" {
    run ! xrdcp -f "$CACHE_URL//$F.missing" out.dat
}

@test "a failed read of a file which does not exist leaves nothing in the cache" {
    xrdcp -f "$CACHE_URL//$F.missing" out.dat || true
    run ! test -e "$CACHE_DATA/$F.missing" -o -e "$(cinfo "$F.missing")"
}
