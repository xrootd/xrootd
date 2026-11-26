#!/usr/bin/env bash

# Fireflies for an XRootD transfer. The tag is scitag.flow on the URL. The
# server is the source of the flow and the client is the destination, so a
# download's file bytes are usage.sent.

load ../setup/server.bash
load firefly.bash

setup_file() {
    export SIZE=$((1024 * 1024))
    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" "$SIZE"
    start_collector firefly
    # Section 1.1 sends a second copy of each firefly to the peer on UDP 10514.
    start_collector peer 10514
    start_server pmark.cfg
    cp "$REF" "$SERVER_DATA/ref.dat"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    OFF=$(firefly_offset "$FIREFLY_RECORDS")
}

@test "a download marked 1024 reports experiment 16 and activity 0" {
    xrdcp "$SERVER_URL4//ref.dat?scitag.flow=1024" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 16 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
}

@test "a download marked 144abc reports the out-of-range mark" {
    xrdcp "$SERVER_URL4//ref.dat?scitag.flow=144abc" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 0 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
}
