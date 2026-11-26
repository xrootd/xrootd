#!/usr/bin/env bash

# Fireflies for an XRootD third-party copy. The client puts scitag.flow on
# the destination URL, which is the open that starts a pull. The file moves
# on the socket the destination's copy program opens to the source, so the
# source server emits the firefly and the destination's control connection
# does not.

load ../setup/server.bash
load firefly.bash

setup_file() {
    export SIZE=$((1024 * 1024))
    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" "$SIZE"
    start_collector srcfly
    start_collector dstfly

    local saved=$FIREFLY_PORT
    export FIREFLY_PORT=$SRCFLY_PORT
    start_server src tpc.cfg pmark.cfg
    export FIREFLY_PORT=$DSTFLY_PORT
    start_server dst tpc.cfg pmark.cfg
    export FIREFLY_PORT=$saved
    cp "$REF" "$SRC_DATA/ref.dat"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
}

@test "a pull carries scitag.flow on the socket that moves the file" {
    local src_off dst_off dest

    dest="/t${BATS_TEST_NUMBER}.dat"
    src_off=$(firefly_offset "$SRCFLY_RECORDS")
    dst_off=$(firefly_offset "$DSTFLY_RECORDS")
    xrdcp -T only "$SRC_URL4//ref.dat" "$DST_URL4/$dest?scitag.flow=144"
    run -0 cmp "$REF" "$DST_DATA$dest"
    firefly_check "$SRCFLY_RECORDS" "$src_off" 2 16 1 sent "$SIZE" \
        --src-port "$SRC_PORT" --application xrdcp
    firefly_unchanged "$DSTFLY_RECORDS" "$dst_off"
}

@test "a flow id named inside another kept key does not replace the open's id" {
    # copyCGI keeps a note. The name scitag.flow inside that note is not a
    # flow id, so the id from the open is the one that travels with the file.
    local src_off dst_off dest

    dest="/t${BATS_TEST_NUMBER}.dat"
    src_off=$(firefly_offset "$SRCFLY_RECORDS")
    dst_off=$(firefly_offset "$DSTFLY_RECORDS")
    xrdcp -T only "$SRC_URL4//ref.dat" \
        "$DST_URL4/$dest?scitag.flow=144&tpc.scgi=note=scitag.flow=1024"
    run -0 grep -F 'tpc.scgi=note=scitag.flow=1024' "$XRD_LOGFILE"
    run -0 cmp "$REF" "$DST_DATA$dest"
    firefly_check "$SRCFLY_RECORDS" "$src_off" 2 16 1 sent "$SIZE" \
        --src-port "$SRC_PORT" --application xrdcp
    firefly_unchanged "$DSTFLY_RECORDS" "$dst_off"
}

@test "a pull uses the last scitag.flow when an earlier one is set" {
    local src_off dst_off dest

    dest="/t${BATS_TEST_NUMBER}.dat"
    src_off=$(firefly_offset "$SRCFLY_RECORDS")
    dst_off=$(firefly_offset "$DSTFLY_RECORDS")
    # XrdOucEnv keeps 144 and drops the empty value. The last key is the
    # empty one, so the source is marked 0.
    xrdcp -T only "$SRC_URL4//ref.dat" \
        "$DST_URL4/$dest?scitag.flow=144&scitag.flow="
    run -0 cmp "$REF" "$DST_DATA$dest"
    firefly_check "$SRCFLY_RECORDS" "$src_off" 0 0 1 sent "$SIZE" \
        --src-port "$SRC_PORT" --application xrdcp
    firefly_unchanged "$DSTFLY_RECORDS" "$dst_off"
}

@test "a pull with an empty scitag.flow is marked 0" {
    local src_off dst_off dest

    dest="/t${BATS_TEST_NUMBER}.dat"
    src_off=$(firefly_offset "$SRCFLY_RECORDS")
    dst_off=$(firefly_offset "$DSTFLY_RECORDS")
    xrdcp -T only "$SRC_URL4//ref.dat" "$DST_URL4/$dest?scitag.flow="
    run -0 cmp "$REF" "$DST_DATA$dest"
    firefly_check "$SRCFLY_RECORDS" "$src_off" 0 0 1 sent "$SIZE" \
        --src-port "$SRC_PORT" --application xrdcp
    firefly_unchanged "$DSTFLY_RECORDS" "$dst_off"
}

@test "a pull with a non-numeric scitag.flow is marked 0" {
    local src_off dst_off dest

    dest="/t${BATS_TEST_NUMBER}.dat"
    src_off=$(firefly_offset "$SRCFLY_RECORDS")
    dst_off=$(firefly_offset "$DSTFLY_RECORDS")
    xrdcp -T only "$SRC_URL4//ref.dat" "$DST_URL4/$dest?scitag.flow=144abc"
    run -0 cmp "$REF" "$DST_DATA$dest"
    firefly_check "$SRCFLY_RECORDS" "$src_off" 0 0 1 sent "$SIZE" \
        --src-port "$SRC_PORT" --application xrdcp
    firefly_unchanged "$DSTFLY_RECORDS" "$dst_off"
}

@test "a flow id inside a dropped tpc.scgi key does not replace the open's id" {
    # copyCGI discards xrd.* keys. The name scitag.flow inside that discarded
    # text is not a flow id, so the id from the open is the one that travels
    # with the file.
    local src_off dst_off dest

    dest="/t${BATS_TEST_NUMBER}.dat"
    src_off=$(firefly_offset "$SRCFLY_RECORDS")
    dst_off=$(firefly_offset "$DSTFLY_RECORDS")
    xrdcp -T only "$SRC_URL4//ref.dat" \
        "$DST_URL4/$dest?scitag.flow=144&tpc.scgi=xrd.note=scitag.flow=1024"
    run -0 grep -F 'tpc.scgi=xrd.note=scitag.flow=1024' "$XRD_LOGFILE"
    run -0 cmp "$REF" "$DST_DATA$dest"
    firefly_check "$SRCFLY_RECORDS" "$src_off" 2 16 1 sent "$SIZE" \
        --src-port "$SRC_PORT" --application xrdcp
    firefly_unchanged "$DSTFLY_RECORDS" "$dst_off"
}
