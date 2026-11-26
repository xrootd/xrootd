#!/usr/bin/env bash

# Fireflies for HTTP. A GET is http-get: the server is the source and the
# file bytes are usage.sent. A PUT is http-put: the supplier is the source
# and the file bytes are usage.sent.
#
# pmark.cfg reads $FIREFLY_PORT. The GET and PUT server uses the firefly
# collector.

load ../setup/server.bash
load firefly.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    export SIZE=$((1024 * 1024))
    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" "$SIZE"

    start_collector firefly
    start_server http.cfg pmark.cfg
    cp "$REF" "$SERVER_DATA/ref.dat"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    OFF=$(firefly_offset "$FIREFLY_RECORDS")
}

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
