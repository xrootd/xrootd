#!/usr/bin/env bash

# The host security protocol, which authenticates the host of the client. The
# tests share one server, which accepts only host, and a server with the
# security library but no protocol.

load ../setup/server.bash

export XrdSecPROTOCOL=host

setup_file() {
    start_server server server host
    start_server implicit server-nosec implicit.cfg
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    FILE="//t${BATS_TEST_NUMBER}"
    echo 'host protocol' > local.txt
}

# The server knows the host from the connection, thus it asks the client for
# no credentials: its security token is empty.
@test "the server asks a client of the host protocol for no credentials" {
    xrdfs "$SERVER_URL" query config version
    run -0 grep -E 'sec_getParms: localhost sectoken=$' <(server_log server)
}

@test "an upload with host authentication succeeds" {
    run -0 xrdcp local.txt "$SERVER_URL/$FILE"
}

@test "a download with host authentication returns the uploaded file" {
    xrdcp local.txt "$SERVER_URL/$FILE"
    xrdcp "$SERVER_URL/$FILE" copy.txt
    run -0 cmp local.txt copy.txt
}

@test "a client restricted to unix logs in, as the server asks for nothing" {
    XrdSecPROTOCOL=unix run -0 xrdfs "$SERVER_URL" query config version
}

@test "xrdfs cat with host authentication prints the file" {
    cp local.txt "$SERVER_DATA/$FILE"
    run -0 xrdfs "$SERVER_URL" cat "$FILE"
    assert_output 'host protocol'
}

@test "xrdfs rm with host authentication removes the file" {
    cp local.txt "$SERVER_DATA/$FILE"
    xrdfs "$SERVER_URL" rm "$FILE"
    run ! test -e "$SERVER_DATA/$FILE"
}

@test "a server without a security protocol warns that only host is available" {
    run -0 grep -F 'No protocols defined; only host authentication available.' "$IMPLICIT_LOG"
}

@test "a server without a security protocol accepts host authentication" {
    run -0 xrdcp local.txt "$IMPLICIT_URL/$FILE"
}
