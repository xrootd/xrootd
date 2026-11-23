#!/usr/bin/env bash

# The Tape REST API of the HTTPS origin of xrdclhttp.bash, with xrdfs prepare
# and the endpoints of the API. Each test has its own file on tape, which is
# not on disk at the start. The tests share the servers.

load xrdclhttp.bash

# The tests use only HTTPS, where XrdClHttp would present a grid proxy of the
# user, which the test CA cannot verify
export X509_USER_PROXY=/dev/null/x509

setup_file() {
    require_commands curl python3
    start_https_cache
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    rm -f "$TAPE/control.json"
    P="/test/tape/t${BATS_TEST_NUMBER}"
    printf '%s\n' 'tape contents' > "$TAPE/archive$P"
    URL="$ORIGIN_HTTPS$P"
}

# Request the endpoint of the API with the read token and the curl
# arguments, with the body in body.txt and the headers in headers.txt, and
# print the HTTP status
api() {
    local endpoint=$1
    shift
    curl --silent --show-error --cacert "$CREDENTIALS/ca.pem" -o body.txt -D headers.txt -w '%{http_code}' \
        -H "Authorization: Bearer $(<"$READ_TOKEN")" "$@" "$ORIGIN_HTTPS/api/v1/$endpoint"
}

# The archive information of the file of the test
archive_info() {
    local status
    status=$(api archiveinfo -H 'Content-Type: application/json' --data "{\"paths\":[\"$P\"]}") || return 1
    cat body.txt
    [[ "$status" == 200 ]]
}

# Stage the file of the test, and print the identifier of the request
stage() {
    xrdfs prepare -s "$URL"
}

# Wait until the mock recall completes. A permanent failure ends the poll.
poll_prepare() {
    local id=$1 response attempt
    for ((attempt=0; attempt<60; ++attempt)); do
        response=$(xrdfs "$ORIGIN_HTTPS" query prepare "$id") || return 1
        if [[ "$response" == *'"state":"COMPLETED"'* ]]; then
            printf '%s\n' "$response"
            return 0
        fi
        if [[ "$response" == *'"state":"FAILED"'* || "$response" == *'"state":"CANCELLED"'* ]]; then
            break
        fi
        sleep 1
    done
    printf '%s\n' "$response"
    return 1
}

stage_completed() {
    local id
    id=$(stage) || return 1
    poll_prepare "$id" >&2 || return 1
    printf '%s\n' "$id"
}

poll_locality() {
    local locality=$1 response attempt
    for ((attempt=0; attempt<60; ++attempt)); do
        response=$(archive_info) || return 1
        if [[ "$response" == *"\"locality\":\"$locality\""* ]]; then
            printf '%s\n' "$response"
            return 0
        fi
        sleep 1
    done
    printf '%s\n' "$response"
    return 1
}

mock_control() {
    printf '%s\n' "$1" > "$TAPE/control.json.tmp"
    mv -f "$TAPE/control.json.tmp" "$TAPE/control.json"
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
    run -0 poll_prepare "$id"
    assert_output --partial '"state":"COMPLETED"'
}

@test "query prepare reports the path of the stage request" {
    id=$(stage)
    run -0 poll_prepare "$id"
    assert_output --partial "\"path\":\"$P\""
}

@test "a staged file has the content of the file on tape" {
    stage_completed
    run -0 read_file
    assert_output 'tape contents'
}

@test "archiveinfo reports a staged file on disk and tape" {
    stage_completed
    run -0 archive_info
    assert_output --partial '"locality":"DISK_AND_TAPE"'
}

@test "a cancel of a stage request with a path which is not in it fails with 400" {
    id=$(stage_completed)
    run -0 api "stage/$id/cancel" -H 'Content-Type: application/json' \
        --data "{\"paths\":[\"$P\",\"/test/tape/missing\"]}"
    assert_output 400
}

@test "a staged file can be read after xrdfs prepare -a of its request" {
    id=$(stage_completed)
    xrdfs prepare -a "$id" "$URL"
    run -0 read_file
    assert_output 'tape contents'
}

@test "archiveinfo reports a file evicted with xrdfs prepare -e on tape only" {
    id=$(stage_completed)
    xrdfs prepare -e "$id" "$URL"
    run -0 poll_locality TAPE
    assert_output --partial '"locality":"TAPE"'
}

@test "a file evicted with xrdfs prepare -e fails to read" {
    id=$(stage_completed)
    xrdfs prepare -e "$id" "$URL"
    poll_locality TAPE
    run ! read_file
}

@test "a DELETE of a stage request returns 200" {
    id=$(stage_completed)
    run -0 api "stage/$id" --request DELETE
    assert_output 200
}

@test "a stage request returns 404 after its DELETE" {
    id=$(stage_completed)
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

@test "Tape REST reads a body spanning TLS records and the HTTP buffer" {
    # Valid JSON with padding larger than the protocol's 1 MiB buffer.
    printf '%1048600s' '' > large.json
    printf '{"paths":["%s"]}' "$P" >> large.json

    run -0 api archiveinfo --max-time 30 -H 'Content-Type: application/json' \
        -H 'Expect: 100-continue' --data-binary @large.json
    assert_output 200
}

@test "Tape REST reauthorizes ID-only operations on the same TLS connection" {
    local id url
    id=$(stage)
    url="$ORIGIN_HTTPS/api/v1/stage/$id"

    run -0 curl --silent --show-error --max-time 30 --cacert "$CREDENTIALS/ca.pem" \
        --header "Authorization: Bearer $(<"$READ_TOKEN")" \
        --output /dev/null --write-out '%{http_code}:%{num_connects}\n' "$url" \
        --next --silent --show-error --max-time 30 --cacert "$CREDENTIALS/ca.pem" \
        --output /dev/null --write-out '%{http_code}:%{num_connects}\n' "$url" \
        --next --silent --show-error --max-time 30 --cacert "$CREDENTIALS/ca.pem" \
        --header "Authorization: Bearer $(<"$READ_TOKEN")" \
        --output /dev/null --write-out '%{http_code}:%{num_connects}\n' "$url"
    assert_output $'200:1\n403:0\n200:0'
}

@test "xrdfs stage recovers from a lost backend acknowledgement" {
    mock_control '{"failAfterAccept": true}'
    local id
    id=$(stage)

    run -0 poll_prepare "$id"
    assert_output --partial '"state":"COMPLETED"'
}

@test "xrdfs stage recovers after the origin restarts" {
    mock_control '{"queryFailure": true}'
    local id
    id=$(stage)

    run -0 xrdfs "$ORIGIN_HTTPS" query prepare "$id"
    assert_output --partial '"state":"SUBMITTED"'

    restart_server origin
    mock_control '{}'

    run -0 poll_prepare "$id"
    assert_output --partial '"state":"COMPLETED"'
}
