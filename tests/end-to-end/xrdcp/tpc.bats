#!/usr/bin/env bash

# Third-party copy over root:// between two paths of one server. The tests
# share the server, with the file /ref.dat on it.

load ../setup/server.bash

# No server runs on NO_SERVER_PORT
GONE_URL="root://localhost:$NO_SERVER_PORT"

setup_file() {
    start_server server server tpc

    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" 1000003
    cp "$REF" "$SERVER_DATA/ref.dat"
}

teardown_file() {
    stop_servers
}

setup() {
    TARGET="/t${BATS_TEST_NUMBER}.dat"
}

@test "-T only copies the file" {
    xrdcp -T only "$SERVER_URL//ref.dat" "$SERVER_URL/$TARGET"
    run -0 cmp "$REF" "$SERVER_DATA$TARGET"
}

@test "-T only verifies the crc32c checksum" {
    run -0 xrdcp -T only --cksum crc32c "$SERVER_URL//ref.dat" "$SERVER_URL/$TARGET"
}

@test "-T only verifies the adler32 checksum" {
    run -0 xrdcp -T only --cksum adler32 "$SERVER_URL//ref.dat" "$SERVER_URL/$TARGET"
}

@test "-T only with a wrong expected checksum fails" {
    run ! xrdcp -T only --cksum adler32:00000001 "$SERVER_URL//ref.dat" "$SERVER_URL/$TARGET"
    assert_output --partial 'CheckSum error'
}

@test "-T only from a server which does not run fails with a connection error" {
    XRD_CONNECTIONRETRY=1 run ! xrdcp -T only "$GONE_URL//ref.dat" "$SERVER_URL/$TARGET"
    assert_output --partial 'Connection error'
}

@test "-T only to a server which does not run fails with a connection error" {
    XRD_CONNECTIONRETRY=1 run ! xrdcp -T only "$SERVER_URL//ref.dat" "$GONE_URL/$TARGET"
    assert_output --partial 'Connection error'
}
