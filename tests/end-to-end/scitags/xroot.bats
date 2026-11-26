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

@test "a data open is marked when tpc.stage is only text in another value" {
    # The exclusion is the CGI key tpc.stage, value copy. A note that
    # happens to contain that text still moves the file on this socket.
    xrdcp "$SERVER_URL4//ref.dat?note=tpc.stage=copy&scitag.flow=144" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp
}

@test "a download via 127.0.0.2 sends the port 10514 firefly from that address" {
    local peer new addr
    local -a got

    [[ "$(uname)" == Linux ]] ||
        skip 'the firefly source address is set with a Linux control message'
    python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.2", 0))' \
        || skip '127.0.0.2 is not configured'
    [[ "$PEER_PORT" -eq 10514 ]]
    # The client connects to 127.0.0.2 and its own address stays 127.0.0.1.
    # The server's end of the flow, and the datagram's source, are 127.0.0.2.
    # The source line is written before the datagram, so it can be one ahead.
    # The sources of these datagrams are the last lines, one per new datagram.
    peer=$(firefly_offset "$PEER_RECORDS")
    xrdcp "root://127.0.0.2:${SERVER_PORT}//ref.dat?scitag.flow=144" copy.dat
    run -0 cmp "$REF" copy.dat
    firefly_check "$FIREFLY_RECORDS" "$OFF" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp \
        --src-ip 127.0.0.2 --dst-ip 127.0.0.1
    firefly_check "$PEER_RECORDS" "$peer" 2 16 1 sent "$SIZE" \
        --src-port "$SERVER_PORT" --application xrdcp \
        --src-ip 127.0.0.2 --dst-ip 127.0.0.1
    new=$(tail -c +$((peer + 1)) "$PEER_RECORDS" | wc -l | tr -d '[:space:]')
    mapfile -t got < <(tail -n "$new" "${PEER_RECORDS}.from")
    [[ "$new" -ge 1 && ${#got[@]} -eq "$new" ]]
    for addr in "${got[@]}"; do
        [[ "$addr" == 127.0.0.2 ]]
    done
}
