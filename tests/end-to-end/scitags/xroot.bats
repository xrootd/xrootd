#!/usr/bin/env bash

# Fireflies for an XRootD transfer. The tag is scitag.flow on the URL. The
# server is the source of the flow and the client is the destination, so a
# download's file bytes are usage.sent and an upload's are usage.received.

load ../setup/server.bash
load firefly.bash

setup_file() {
    export SIZE=$((1024 * 1024))
    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" "$SIZE"
    start_collector firefly
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

@test "a download marked 144 counts the file in usage.sent" {
    xrdcp "$SERVER_URL4//ref.dat?scitag.flow=144" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT"
}

@test "an upload marked 144 counts the file in usage.received" {
    local path="/t${BATS_TEST_NUMBER}.dat"

    xrdcp "$REF" "$SERVER_URL4/$path?scitag.flow=144"
    run -0 cmp "$REF" "$SERVER_DATA$path"
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 received "$SIZE" \
        --src-port "$SERVER_PORT"
}

@test "a download with 3 streams marks the session and each substream" {
    local binds

    xrdcp --streams 3 "$SERVER_URL4//ref.dat?scitag.flow=144" copy.dat
    run -0 cmp "$REF" copy.dat
    # The client logs kXR_bind successful for each substream. The session
    # connection is marked as well. The file is spread across the sockets,
    # so the sum of usage.sent covers it.
    binds=$(grep -c 'kXR_bind successful' "$XRD_LOGFILE" || true)
    [[ "$binds" -eq 3 ]]
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 "$((binds + 1))" sent "$SIZE" \
        --src-port "$SERVER_PORT" --sum
}

@test "a download marked 1024 reports experiment 16 and activity 0" {
    xrdcp "$SERVER_URL4//ref.dat?scitag.flow=1024" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 16 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT"
}

@test "a download marked 64 reports the out-of-range mark" {
    xrdcp "$SERVER_URL4//ref.dat?scitag.flow=64" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 0 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT"
}

@test "a download without scitag.flow sends no firefly" {
    xrdcp "$SERVER_URL4//ref.dat" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_unchanged "$FIREFLY_RECORDS" "$OFF"
}
