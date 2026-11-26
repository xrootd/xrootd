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

@test "a download marked 144 counts the file in usage.sent" {
    xrdcp "$SERVER_URL4//ref.dat?scitag.flow=144" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
}

@test "an upload marked 144 counts the file in usage.received" {
    local path="/t${BATS_TEST_NUMBER}.dat"

    xrdcp "$REF" "$SERVER_URL4/$path?scitag.flow=144"
    run -0 cmp "$REF" "$SERVER_DATA$path"
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 received "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
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
        --src-port "$SERVER_PORT" --application xrdcp --sum
}

@test "a download marked 1024 reports experiment 16 and activity 0" {
    xrdcp "$SERVER_URL4//ref.dat?scitag.flow=1024" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 16 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
}

@test "a download marked 64 reports the out-of-range mark" {
    xrdcp "$SERVER_URL4//ref.dat?scitag.flow=64" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 0 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
}

@test "a download marked 144abc reports the out-of-range mark" {
    xrdcp "$SERVER_URL4//ref.dat?scitag.flow=144abc" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 0 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
}

@test "a data open is marked when tpc.stage is only text in another value" {
    # The exclusion is the CGI key tpc.stage, value copy. A note that
    # happens to contain that text still moves the file on this socket.
    xrdcp "$SERVER_URL4//ref.dat?note=tpc.stage=copy&scitag.flow=144" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
}

@test "a download without scitag.flow sends no firefly" {
    xrdcp "$SERVER_URL4//ref.dat" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_unchanged "$FIREFLY_RECORDS" "$OFF"
}

@test "an IPv6 download reports the addresses of that flow" {
    require_ipv6
    xrdcp "$SERVER_URL6//ref.dat?scitag.flow=144" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp \
        --afi ipv6 --address '::1'
}

@test "the copy sent to the peer on port 10514 matches the collector" {
    local peer

    [[ "$PEER_PORT" -eq 10514 ]]
    peer=$(firefly_offset "$PEER_RECORDS")
    xrdcp "$SERVER_URL4//ref.dat?scitag.flow=144" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
    firefly_check "$PEER_RECORDS" "$peer" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
}

@test "a second open on the same connection carries the second SciTag" {
    # pmDone still keeps the first tag. Section 3.5.1 expects the second
    # CGI to be honored. This test stays off until that is implemented.
    skip 'pmDone still keeps the first tag; section 3.5.1 expects the second CGI to be honored; this test stays off until that is implemented'

    # One xrdfs session reuses the socket. The second cat asks for a
    # different flow id. xrdfs still exits 0 when a command fails, so each
    # payload is checked on its own. XRD_REQUESTTIMEOUT bounds a stuck transfer.
    xrdfs "$SERVER_URL4" <<EOF
cat -o first.dat /ref.dat?scitag.flow=144
cat -o second.dat /ref.dat?scitag.flow=1024
exit
EOF
    run -0 cmp "$REF" first.dat
    run -0 cmp "$REF" second.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdfs --select
    firefly_check "$FIREFLY_RECORDS" "$OFF" 16 0 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdfs --select
}
