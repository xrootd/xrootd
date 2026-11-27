#!/usr/bin/env bash

# The client stops its threads when the process exits. OpenSSL frees its data
# in an exit handler, and the poller thread of a substream which still did its
# TLS handshake then used the freed data. xrdfs crashed with SIGSEGV after a
# fast request, mostly on musl. Each test runs many clients in parallel, as
# one exit hits the race only sometimes.

load ../setup/server.bash

setup() {
    start_server tls.cfg
    export X509_CERT_FILE="$CREDENTIALS/ca.pem"
    echo content > "$SERVER_DATA/file"
    cd "$BATS_TEST_TMPDIR"
}

teardown() {
    stop_servers
}

# Run 10 stats, one after the other, and print the exit status of each
stats() {
    local i rc

    for (( i = 0; i < 10; i++ )); do
        rc=0
        xrdfs "roots://localhost:$SERVER_PORT" stat /file > /dev/null || rc=$?
        echo "$rc"
    done
}

@test "parallel stats over TLS with 15 substreams exit with status 0" {
    local n

    # the substreams connect at once, and do their TLS handshakes during the
    # exit of a client which only does a stat
    export XRD_SUBSTREAMSPERCHANNEL=15

    # The 256 connections at once overflow the listen queue of the server on
    # macOS, which has a limit of 128 (kern.ipc.somaxconn). The kernel then
    # drops or resets some connections. When the first connection of a client
    # fails, the client connects again at the end of the connection window,
    # thus a short window keeps the retries within XRD_REQUESTTIMEOUT.
    export XRD_CONNECTIONWINDOW=2
    for n in {1..16}; do
        stats > "status.$n" &
    done
    wait

    run -0 sort -n status.1 status.{2..16}
    assert_equal "$(grep -cx 0 <<< "$output")" 160
}
