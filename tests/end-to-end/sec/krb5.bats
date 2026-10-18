#!/usr/bin/env bash

# The krb5 security protocol, with a KDC of the test file. The tests share the
# KDC and one server, which accepts only krb5.

load ../setup/server.bash
load ../setup/kerberos.bash

export XrdSecPROTOCOL=krb5

setup_file() {
    [[ "$OSTYPE" != darwin* ]] || skip "the system Kerberos of macOS has no KDC"
    start_kdc
    kinit xrootd@XROOTD.ORG <<< xrootd > /dev/null
    start_server server server krb5
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    FILE="//t${BATS_TEST_NUMBER}"
    echo 'krb5 protocol' > local.txt
}

@test "kinit gets a ticket for the client" {
    run -0 klist
    assert_output --partial "xrootd@XROOTD.ORG"
}

@test "the client authenticates with krb5" {
    xrdfs "$SERVER_URL" query config version
    run -0 grep -F 'Authenticated with krb5' "$XRD_LOGFILE"
}

@test "an upload with krb5 authentication succeeds" {
    run -0 xrdcp local.txt "$SERVER_URL/$FILE"
}

@test "a download with krb5 authentication returns the uploaded file" {
    xrdcp local.txt "$SERVER_URL/$FILE"
    xrdcp "$SERVER_URL/$FILE" copy.txt
    run -0 cmp local.txt copy.txt
}

@test "a client without a ticket cannot log in" {
    KRB5CCNAME="FILE:$BATS_TEST_TMPDIR/no-ticket" run ! xrdfs "$SERVER_URL" query config version
}
