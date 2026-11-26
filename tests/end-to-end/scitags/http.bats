#!/usr/bin/env bash

# Fireflies for HTTP and HTTP-TPC. Clients use 127.0.0.1. A GET is http-get:
# the server is the source and the file bytes are usage.sent. A PUT, and a
# TPC pull marked on the active server, are http-put: the supplier is the
# source and the file bytes are usage.sent.
#
# The first tests are the regressions: each one failed before the change that
# now makes it pass. The rest cover specs/scitags.txt section 3.5.2.
#
# pmark.cfg reads $FIREFLY_PORT. The GET and PUT server uses the firefly
# collector. Each TPC server gets its own collector, so a test can tell
# which server emitted.

load ../setup/server.bash
load firefly.bash

# The same search as require_plugin, without skipping the GET and PUT tests.
have_plugin() {
    local name=$1 lib

    [[ -n "${BINARY_DIR:-}" ]] || return 0
    for lib in "$BINARY_DIR"/lib*/"$name"{,-*}.{so,dylib}; do
        [[ -e "$lib" ]] && return 0
    done
    return 1
}

setup_file() {
    local saved

    require_plugin libXrdHttp
    require_commands curl
    export SIZE=$((1024 * 1024))
    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" "$SIZE"

    start_collector firefly
    start_server http.cfg pmark.cfg
    cp "$REF" "$SERVER_DATA/ref.dat"

    if have_plugin libXrdHttpTPC; then
        start_collector srcfly
        start_collector dstfly
        saved=$FIREFLY_PORT
        export FIREFLY_PORT=$SRCFLY_PORT
        start_server src http.cfg http-tpc.cfg pmark.cfg
        export FIREFLY_PORT=$DSTFLY_PORT
        start_server dst http.cfg http-tpc.cfg pmark.cfg
        export FIREFLY_PORT=$saved
        cp "$REF" "$SRC_DATA/ref.dat"
    fi
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    OFF=$(firefly_offset "$FIREFLY_RECORDS")
}

# http_copy <source path> <dest path> [curl argument...]
#
# COPY onto the active server. Prints the HTTP status and writes body.txt.
http_copy() {
    local src=$1 dest=$2
    shift 2
    curl --silent --show-error -o body.txt -w '%{http_code}' -X COPY \
        -H "Source: http://127.0.0.1:${SRC_PORT}${src}" \
        "$@" \
        "http://127.0.0.1:${DST_PORT}${dest}"
}

# http_push <source path> <dest path> [curl argument...]
#
# COPY from the source server, which pushes the file. Prints the HTTP status.
http_push() {
    local src=$1 dest=$2
    shift 2
    curl --silent --show-error -o body.txt -w '%{http_code}' -X COPY \
        -H "Destination: http://127.0.0.1:${DST_PORT}${dest}" \
        "$@" \
        "http://127.0.0.1:${SRC_PORT}${src}"
}

# --- Regressions -----------------------------------------------------------

@test "a GET with SciTag 1024 reports experiment 16 and activity 0" {
    curl --silent --show-error --fail -H 'SciTag: 1024' -o copy.dat \
        "http://127.0.0.1:${SERVER_PORT}/ref.dat"
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 16 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application http-get
}
@test "a PUT with SciTag 1024 reports experiment 16 and activity 0" {
    local path="/t${BATS_TEST_NUMBER}.dat"

    curl --silent --show-error --fail -H 'SciTag: 1024' -T "$REF" \
        "http://127.0.0.1:${SERVER_PORT}${path}"
    run -0 cmp "$REF" "$SERVER_DATA$path"
    firefly_check "$FIREFLY_RECORDS" "$OFF" 16 0 1 sent "$SIZE" \
        --dst-port "$SERVER_PORT" --application http-put
}
@test "a GET with SciTag 144abc reports the out-of-range mark" {
    curl --silent --show-error --fail -H 'SciTag: 144abc' -o copy.dat \
        "http://127.0.0.1:${SERVER_PORT}/ref.dat"
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 0 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application http-get
}
@test "a later GET on an unmarked connection carries the SciTag" {
    # The first request has no tag, so the socket is not marked. The second
    # names one, and that mark has to start on the same connection.
    curl --http1.1 --silent --show-error --fail \
        -H 'Connection: keep-alive' \
        -o first.dat "http://127.0.0.1:${SERVER_PORT}/ref.dat" \
        --next \
        -H 'Connection: keep-alive' -H 'SciTag: 144' \
        -o second.dat "http://127.0.0.1:${SERVER_PORT}/ref.dat"
    run -0 cmp "$REF" first.dat
    run -0 cmp "$REF" second.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application http-get
}
@test "a second GET on the same connection carries the second SciTag" {
    # --next reuses the connection. The second request names another flow,
    # so the first firefly ends and a new one starts on that socket.
    curl --http1.1 --silent --show-error --fail \
        -H 'Connection: keep-alive' -H 'SciTag: 144' \
        -o first.dat "http://127.0.0.1:${SERVER_PORT}/ref.dat" \
        --next \
        -H 'Connection: keep-alive' -H 'SciTag: 1024' \
        -o second.dat "http://127.0.0.1:${SERVER_PORT}/ref.dat"
    run -0 cmp "$REF" first.dat
    run -0 cmp "$REF" second.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application http-get --select
    firefly_check "$FIREFLY_RECORDS" "$OFF" 16 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application http-get --select
    # The two tags are one socket. Two connections would not share a port.
    [[ "$(firefly_endpoint "$FIREFLY_RECORDS" "$OFF" 2 16)" == \
       "$(firefly_endpoint "$FIREFLY_RECORDS" "$OFF" 16 0)" ]]
}
@test "a second GET with the same SciTag keeps one firefly" {
    curl --http1.1 --silent --show-error --fail \
        -H 'Connection: keep-alive' -H 'SciTag: 144' \
        -o first.dat "http://127.0.0.1:${SERVER_PORT}/ref.dat" \
        --next \
        -H 'Connection: keep-alive' -H 'SciTag: 144' \
        -o second.dat "http://127.0.0.1:${SERVER_PORT}/ref.dat"
    run -0 cmp "$REF" first.dat
    run -0 cmp "$REF" second.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$((2 * SIZE))" \
        --src-port "$SERVER_PORT" --application http-get
}
@test "a pull with both SciTag headers keeps the SciTag of the COPY" {
    local src_off dst_off dest

    require_plugin libXrdHttpTPC
    dest="/t${BATS_TEST_NUMBER}.dat"
    src_off=$(firefly_offset "$SRCFLY_RECORDS")
    dst_off=$(firefly_offset "$DSTFLY_RECORDS")
    # 1024 would be experiment 16. Section 3.5.2 says it is ignored.
    run -0 http_copy /ref.dat "$dest" -H 'SciTag: 144' -H 'TransferHeaderSciTag: 1024'
    assert_output 202
    run -0 grep '^success' body.txt
    run -0 cmp "$SRC_DATA/ref.dat" "$DST_DATA$dest"
    firefly_check "$DSTFLY_RECORDS" "$dst_off" 2 16 1 sent "$SIZE" \
        --src-port "$SRC_PORT" --application http-put
    firefly_unchanged "$SRCFLY_RECORDS" "$src_off"
}
@test "a push with SciTag 144 is marked by the source server" {
    local src_off dst_off dest

    require_plugin libXrdHttpTPC
    dest="/t${BATS_TEST_NUMBER}.dat"
    src_off=$(firefly_offset "$SRCFLY_RECORDS")
    dst_off=$(firefly_offset "$DSTFLY_RECORDS")
    run -0 http_push /ref.dat "$dest" -H 'SciTag: 144'
    assert_output 202
    run -0 grep '^success' body.txt
    run -0 cmp "$SRC_DATA/ref.dat" "$DST_DATA$dest"
    # The source server sends the file. Its port on that socket is ephemeral,
    # and the destination's listen port is the destination of the flow.
    firefly_check "$SRCFLY_RECORDS" "$src_off" 2 16 1 sent "$SIZE" \
        --dst-port "$DST_PORT" --application http-get
    firefly_unchanged "$DSTFLY_RECORDS" "$dst_off"
}
