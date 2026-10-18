#!/usr/bin/env bash

# The gsi security protocol, with proxies of the shared client, invalid and
# revoked certificates. The tests share one server, which accepts only gsi and
# requires signed requests.

load ../setup/server.bash

export XrdSecPROTOCOL=gsi
export X509_CERT_DIR="$CREDENTIALS"
export X509_USER_PROXY="$BATS_FILE_TMPDIR/proxy.crt"

setup_file() {
    local name

    for name in client invalid revoked; do
        xrdgsiproxy init -certdir "$CREDENTIALS" -cert "$CREDENTIALS/$name.crt" \
            -key "$CREDENTIALS/$name.key" -out "$BATS_FILE_TMPDIR/$name.proxy" > /dev/null
    done
    mv "$BATS_FILE_TMPDIR/client.proxy" "$X509_USER_PROXY"

    start_server server server tls gsi.cfg
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    FILE="//t${BATS_TEST_NUMBER}"
    echo 'gsi protocol' > local.txt
}

@test "xrdgsitest passes its checks of the shared certificates" {
    # xrdgsitest makes a proxy of its own; the other tests share the one of
    # the file
    X509_USER_CERT="$CREDENTIALS/client.crt" X509_USER_KEY="$CREDENTIALS/client.key" \
        X509_USER_PROXY="$BATS_TEST_TMPDIR/xrdgsitest.proxy" run -0 xrdgsitest
}

@test "the client authenticates with gsi" {
    xrdfs "$SERVER_URL" query config version
    run -0 grep -F 'Authenticated with gsi' "$XRD_LOGFILE"
}

@test "an upload with gsi authentication succeeds" {
    run -0 xrdcp local.txt "$SERVER_URL/$FILE"
}

@test "a download with gsi authentication returns the uploaded file" {
    xrdcp local.txt "$SERVER_URL/$FILE"
    xrdcp "$SERVER_URL/$FILE" copy.txt
    run -0 cmp local.txt copy.txt
}

@test "a proxy given by a path relative to the current directory authenticates" {
    cp "$X509_USER_PROXY" relative.proxy
    X509_USER_PROXY=relative.proxy run -0 xrdfs "$SERVER_URL" query config version
}

@test "the client signs its requests" {
    xrdfs "$SERVER_URL" query config version
    run -0 grep -F 'XrdSecProtect loaded' "$XRD_LOGFILE"
}

@test "the server verifies the signed requests" {
    # the client signs the open of the upload, but not a query
    xrdcp local.txt "$SERVER_URL/$FILE"
    run -0 xrdfs "$SERVER_URL" query stats p
    assert_regex "$output" '<sig><ok>[1-9][0-9]*</ok>'
}

@test "the server rejects no signed request" {
    run -0 xrdfs "$SERVER_URL" query stats p
    assert_regex "$output" '<sig><ok>[0-9]+</ok><bad>0</bad>'
}

@test "a proxy of a certificate without the client extensions cannot log in" {
    X509_USER_PROXY="$BATS_FILE_TMPDIR/invalid.proxy" run ! xrdfs "$SERVER_URL" query config version
}

@test "a proxy of a revoked certificate cannot log in" {
    X509_USER_PROXY="$BATS_FILE_TMPDIR/revoked.proxy" run ! xrdfs "$SERVER_URL" query config version
}

# Neither an RSA signing oracle in the server nor a refusal of an old client
# (protocol version 10600)
@test "the server signs no data of the client and accepts an old client" {
    run -0 xrd-gsi-oracle localhost "$SERVER_PORT" "$CREDENTIALS/host.pem"
}

@test "a proxy which others can read cannot log in over TLS" {
    cp "$X509_USER_PROXY" open.proxy
    chmod 644 open.proxy
    X509_USER_PROXY="$PWD/open.proxy" run ! xrdfs "roots://localhost:$SERVER_PORT" ls /
}

@test "a proxy which others can read fails the TLS handshake" {
    cp "$X509_USER_PROXY" open.proxy
    chmod 644 open.proxy
    X509_USER_PROXY="$PWD/open.proxy" xrdfs "roots://localhost:$SERVER_PORT" ls / || true
    run -0 grep -F 'TLS error' "$XRD_LOGFILE"
}
