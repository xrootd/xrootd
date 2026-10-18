#!/usr/bin/env bash

# The issuance of macaroons over HTTPS, and access with them. The tests share
# one server; its authorization database bounds what a macaroon allows.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdMacaroons
    require_commands curl jq

    start_server server server http tls macaroons macaroons.cfg

    echo 'Hello, macaroons!' > "$SERVER_DATA/hello.txt"
    echo 'Delete me if you can!' > "$SERVER_DATA/deleteme.txt"
    mkdir "$SERVER_DATA/rw"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    N=$BATS_TEST_NUMBER
    echo 'upload attempt' > upload.txt
}

# curl with the shared CA, and the given arguments
https() {
    curl --silent --show-error --fail --cacert "$CREDENTIALS/ca.pem" "$@"
}

# Ask for a macaroon for the path, valid for one hour, with an optional caveat
# (a JSON string), as the client of the shared credentials. Print it.
macaroon() {
    local path=$1 caveats=${2:+,\"caveats\":[$2]}

    https --cert "$CREDENTIALS/client.crt" --key "$CREDENTIALS/client.key" \
        -H 'Content-Type: application/macaroon-request' -X POST \
        -d "{\"validity\":\"PT1H\"$caveats}" "$SERVER_HTTPS$path" | jq -er .macaroon
}

# A POST of a macaroon request with the given body
request() {
    https -H 'Content-Type: application/macaroon-request' -X POST -d "$1" "$SERVER_HTTPS/"
}

@test "a request with a validity issues a macaroon" {
    run -0 macaroon /
    refute_output ''
}

@test "the response gives the lifetime of the macaroon in expires_in" {
    run -0 bats_pipe request '{ "validity": "PT30S" }' \| jq -e '.expires_in == 30'
}

@test "a request without a validity is refused" {
    run ! request '{}'
}

@test "a request with the reserved caveat path is refused" {
    run ! request '{"validity":"PT1H","caveats":["path:/secret"]}'
}

@test "a request with the reserved caveat name is refused" {
    run ! request '{"validity":"PT1H","caveats":["name:root"]}'
}

@test "a request with an unknown caveat is refused" {
    run ! request '{"validity":"PT1H","caveats":["foobar:baz"]}'
}

@test "a DOWNLOAD macaroon reads a file" {
    local token
    token=$(macaroon / '"activity:DOWNLOAD,LIST"')
    run -0 https -H "Authorization: Bearer $token" "$SERVER_HTTPS/hello.txt"
    assert_output 'Hello, macaroons!'
}

@test "a read without a macaroon is allowed by the authorization database" {
    run -0 https "$SERVER_HTTPS/hello.txt"
}

@test "a macaroon for a path cannot read a file outside of it" {
    local token
    token=$(macaroon /subdir/)
    run ! https -H "Authorization: Bearer $token" "$SERVER_HTTPS/hello.txt"
}

@test "an upload without a macaroon is denied" {
    run ! https -X PUT -T upload.txt "$SERVER_HTTPS/upload$N.txt"
}

@test "an UPLOAD macaroon cannot write where the authorization database denies it" {
    local token
    token=$(macaroon / '"activity:MANAGE,UPLOAD"')
    run ! https -H "Authorization: Bearer $token" -X PUT -T upload.txt "$SERVER_HTTPS/upload$N.txt"
}

@test "an UPLOAD macaroon writes where the authorization database allows it" {
    local token
    token=$(macaroon /rw '"activity:MANAGE,UPLOAD"')
    https -H "Authorization: Bearer $token" -X PUT -T upload.txt "$SERVER_HTTPS/rw/upload$N.txt"
    run -0 cmp upload.txt "$SERVER_DATA/rw/upload$N.txt"
}

@test "a DELETE macaroon cannot delete where the authorization database denies it" {
    local token
    token=$(macaroon / '"activity:DELETE"')
    run ! https -H "Authorization: Bearer $token" -X DELETE "$SERVER_HTTPS/deleteme.txt"
}

@test "a MANAGE macaroon cannot make a directory where the authorization database denies it" {
    local token
    token=$(macaroon / '"activity:MANAGE"')
    run ! https -H "Authorization: Bearer $token" -X MKCOL "$SERVER_HTTPS/newdir$N/"
}

@test "a MANAGE macaroon makes a directory where the authorization database allows it" {
    local token
    token=$(macaroon /rw '"activity:MANAGE"')
    https -H "Authorization: Bearer $token" -X MKCOL "$SERVER_HTTPS/rw/newdir$N/"
    run -0 test -d "$SERVER_DATA/rw/newdir$N"
}
