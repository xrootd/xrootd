#!/usr/bin/env bash

# The Tape REST API of the HTTPS origin of xrdclhttp.bash, with xrdfs prepare
# and the endpoints of the API. Each test has its own file on tape, which is
# not on disk at the start. The tests share the servers.

load xrdclhttp.bash

export BEARER_TOKEN_FILE

# The tests use only HTTPS, where XrdClHttp would present a grid proxy of the
# user, which the test CA cannot verify
export X509_USER_PROXY=/dev/null/x509

setup_file() {
    require_commands curl
    start_https_cache
    BEARER_TOKEN_FILE=$WRITE_TOKEN
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    P="/test/tape/t${BATS_TEST_NUMBER}"
    printf '%s\n' 'tape contents' > "$TAPE/archive$P"
    URL="$ORIGIN_HTTPS$P"
}

# Request the endpoint of the API with the write token and the curl
# arguments, with the body in body.txt and the headers in headers.txt, and
# print the HTTP status
api() {
    local endpoint=$1
    shift
    curl --silent --cacert "$CREDENTIALS/ca.pem" -o body.txt -D headers.txt -w '%{http_code}' \
        -H "Authorization: Bearer $(<"$WRITE_TOKEN")" "$@" "$ORIGIN_HTTPS/api/v1/$endpoint"
}

# The archive information of the file of the test
archive_info() {
    api archiveinfo -H 'Content-Type: application/json' --data "{\"paths\":[\"$P\"]}" > /dev/null
    cat body.txt
}

# Stage the file of the test, and print the identifier of the request
stage() {
    xrdfs prepare -s "$URL"
}

# Print the file of the test with the read token
read_file() {
    xrdfs "$ORIGIN_HTTPS" cat "$P?authz=$(<"$READ_TOKEN")"
}

@test "archiveinfo reports a file which is not staged on tape only" {
    run -0 archive_info
    assert_output --partial '"locality":"TAPE"'
}

@test "xrdfs prepare -s returns the UUID of the stage request" {
    run -0 stage
    assert_output --regexp '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'
}

@test "query prepare reports a completed stage request" {
    id=$(stage)
    run -0 xrdfs "$ORIGIN_HTTPS" query prepare "$id"
    assert_output --partial '"state":"COMPLETED"'
}

@test "query prepare reports the path of the stage request" {
    id=$(stage)
    run -0 xrdfs "$ORIGIN_HTTPS" query prepare "$id"
    assert_output --partial "\"path\":\"$P\""
}

@test "a staged file has the content of the file on tape" {
    stage
    run -0 read_file
    assert_output 'tape contents'
}

@test "archiveinfo reports a staged file on disk and tape" {
    stage
    run -0 archive_info
    assert_output --partial '"locality":"DISK_AND_TAPE"'
}

@test "a cancel of a stage request with a path which is not in it fails with 400" {
    id=$(stage)
    run -0 api "stage/$id/cancel" -H 'Content-Type: application/json' \
        --data "{\"paths\":[\"$P\",\"/test/tape/missing\"]}"
    assert_output 400
}

@test "a staged file can be read after xrdfs prepare -a of its request" {
    id=$(stage)
    xrdfs prepare -a "$id" "$URL"
    run -0 read_file
    assert_output 'tape contents'
}

@test "archiveinfo reports a file evicted with xrdfs prepare -e on tape only" {
    id=$(stage)
    xrdfs prepare -e "$id" "$URL"
    run -0 archive_info
    assert_output --partial '"locality":"TAPE"'
}

@test "a file evicted with xrdfs prepare -e fails to read" {
    id=$(stage)
    xrdfs prepare -e "$id" "$URL"
    run ! read_file
}

@test "a DELETE of a stage request returns 200" {
    id=$(stage)
    run -0 api "stage/$id" --request DELETE
    assert_output 200
}

@test "a stage request returns 404 after its DELETE" {
    id=$(stage)
    api "stage/$id" --request DELETE
    run -0 api "stage/$id"
    assert_output 404
}

@test "xrdfs prepare refuses the flags which the Tape REST API does not support" {
    run ! xrdfs prepare -s -w "$URL"
}

@test "an archiveinfo request without paths fails with 400" {
    run -0 api archiveinfo -H 'Content-Type: application/json' --data '{}'
    assert_output 400
}

@test "an error of the Tape REST API has the type of an RFC 7807 problem" {
    api archiveinfo -H 'Content-Type: application/json' --data '{}'
    run -0 grep -qi '^Content-Type: application/problem+json' headers.txt
}

@test "an error of the Tape REST API has its status in the body" {
    api archiveinfo -H 'Content-Type: application/json' --data '{}'
    run -0 grep -q '"status":400' body.txt
}
