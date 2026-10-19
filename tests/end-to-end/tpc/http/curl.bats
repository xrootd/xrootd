#!/usr/bin/env bash

# Third-party copies which curl requests from servers with TLS, ztn for
# root://, and authorization by the tokens of the test issuer. The server
# "pmark" marks packets, and runs with libcheckclose, which aborts it on a
# close of a descriptor which is not open (issue #2889). The servers send
# the g-stream of HTTP requests to a collector. The tests share the servers
# and one token, which grants all paths; each test has its own files.

load ../../setup/server.bash

export X509_CERT_FILE="$CREDENTIALS/ca.pem"
export XrdSecPROTOCOL=ztn

setup_file() {
    require_plugin libXrdHttpTPC
    require_plugin libXrdAccSciTokens
    require_commands curl
    start_issuer
    make_token 'storage.modify:/ storage.create:/ storage.read:/' 1800 > "$BATS_FILE_TMPDIR/token"
    chmod 0600 "$BATS_FILE_TMPDIR/token"
    export BEARER_TOKEN_FILE="$BATS_FILE_TMPDIR/token"

    start_collector monitor
    local snippets=(http tls https-tpc tokens.cfg ztn curl-tpc.cfg monitor)
    start_server src server "${snippets[@]}"
    start_server dst server "${snippets[@]}"
    start_collector firefly
    preload start_server pmark server "${snippets[@]}" pmark.cfg
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    F="/t${BATS_TEST_NUMBER}.dat"
    TOKEN=$(<"$BATS_FILE_TMPDIR/token")
    make_random_file "$SRC_DATA$F" 100003
}

# Run the command with libcheckclose preloaded, if the build has it
preload() {
    local lib
    for lib in "$BINARY_DIR"/lib/libcheckclose.{so,dylib}; do
        [[ -e "$lib" ]] || continue
        if [[ "$lib" == *.so ]]; then
            LD_PRELOAD="$lib" "$@"
        else
            DYLD_INSERT_LIBRARIES="$lib" "$@"
        fi
        return
    done
    "$@"
}

# copy <pull|push> <source URL> <destination URL> [curl argument...]
#
# Request the copy, with the response in body.txt, and print the HTTP status
copy() {
    local mode=$1 src=$2 dst=$3
    shift 3
    local header url

    if [[ "$mode" == pull ]]; then
        header="Source: $src" url=$dst
    else
        header="Destination: $dst" url=$src
    fi

    curl --silent -L -X COPY -o body.txt -w '%{http_code}' --cacert "$CREDENTIALS/ca.pem" \
        -H "Authorization: Bearer $TOKEN" -H "TransferHeaderAuthorization: Bearer $TOKEN" \
        -H "$header" "$@" "$url"
}

# server_checksum <type> <root URL> <path>: the checksum of the file, as the
# server computes it
server_checksum() {
    xrdfs "$2" query checksum "$3?cks.type=$1" | cut -d' ' -f2
}

@test "an HTTP pull copy with tokens returns 202" {
    run -0 copy pull "$SRC_HTTPS$F" "$DST_HTTPS$F"
    assert_output 202
}

@test "an HTTP pull copy with tokens reports success in the body" {
    copy pull "$SRC_HTTPS$F" "$DST_HTTPS$F"
    run -0 grep '^success' body.txt
}

@test "an HTTP pull copy with tokens writes the content of the source" {
    copy pull "$SRC_HTTPS$F" "$DST_HTTPS$F"
    run -0 cmp "$SRC_DATA$F" "$DST_DATA$F"
}

@test "an HTTP push copy with tokens returns 202" {
    run -0 copy push "$SRC_HTTPS$F" "$DST_HTTPS$F"
    assert_output 202
}

@test "an HTTP push copy with tokens reports success in the body" {
    copy push "$SRC_HTTPS$F" "$DST_HTTPS$F"
    run -0 grep '^success' body.txt
}

@test "an HTTP push copy with tokens writes the content of the source" {
    copy push "$SRC_HTTPS$F" "$DST_HTTPS$F"
    run -0 cmp "$SRC_DATA$F" "$DST_DATA$F"
}

@test "the server adler32 of the target of an HTTP pull copy is the one of the source" {
    copy pull "$SRC_HTTPS$F" "$DST_HTTPS$F"
    run -0 server_checksum adler32 "$DST_URL" "$F"
    assert_output "$(checksum adler32 "$SRC_DATA$F")"
}

@test "the server crc32c of the target of an HTTP pull copy is the one of the source" {
    copy pull "$SRC_HTTPS$F" "$DST_HTTPS$F"
    run -0 server_checksum crc32c "$DST_URL" "$F"
    assert_output "$(checksum crc32c "$SRC_DATA$F")"
}

@test "the server crc32c of the target of an HTTP push copy is the one of the source" {
    copy push "$SRC_HTTPS$F" "$DST_HTTPS$F"
    run -0 server_checksum crc32c "$DST_URL" "$F"
    assert_output "$(checksum crc32c "$SRC_DATA$F")"
}

@test "an HTTP pull copy of an empty file creates an empty file" {
    : > "$SRC_DATA$F"
    copy pull "$SRC_HTTPS$F" "$DST_HTTPS$F"
    run -0 test -f "$DST_DATA$F" -a ! -s "$DST_DATA$F"
}

@test "an HTTP push copy of an empty file creates an empty file" {
    : > "$SRC_DATA$F"
    copy push "$SRC_HTTPS$F" "$DST_HTTPS$F"
    run -0 test -f "$DST_DATA$F" -a ! -s "$DST_DATA$F"
}

@test "a roots copy of an empty file with a token creates an empty file" {
    : > "$SRC_DATA$F"
    xrdcp "roots://localhost:$SRC_PORT/$F" "roots://localhost:$DST_PORT/$F"
    run -0 test -f "$DST_DATA$F" -a ! -s "$DST_DATA$F"
}

@test "a roots copy with a token between two servers keeps the content" {
    xrdcp "roots://localhost:$SRC_PORT/$F" "roots://localhost:$DST_PORT/$F"
    run -0 cmp "$SRC_DATA$F" "$DST_DATA$F"
}

@test "an HTTP upload of an empty file with chunked encoding returns 201" {
    : > empty.dat
    run -0 curl --silent -L -X PUT -o /dev/null -w '%{http_code}' --cacert "$CREDENTIALS/ca.pem" \
        -H "Authorization: Bearer $TOKEN" -H 'Transfer-Encoding: chunked' \
        --data-binary @empty.dat "$SRC_HTTPS$F.empty"
    assert_output 201
}

@test "an HTTP pull copy with 101 streams fails with 400" {
    run -0 copy pull "$SRC_HTTPS$F" "$DST_HTTPS$F" -H 'X-Number-Of-Streams: 101'
    assert_output 400
}

@test "an HTTP pull copy of 1 MiB with 4 streams writes the content of the source" {
    make_random_file "$SRC_DATA$F" $(( 1 << 20 ))
    copy pull "$SRC_HTTPS$F" "$DST_HTTPS$F" -H 'X-Number-Of-Streams: 4'
    run -0 cmp "$SRC_DATA$F" "$DST_DATA$F"
}

@test "an HTTP pull copy of 32 MiB with 4 streams writes the content of the source" {
    make_random_file "$SRC_DATA$F" $(( 32 << 20 ))
    copy pull "$SRC_HTTPS$F" "$DST_HTTPS$F" -H 'X-Number-Of-Streams: 4'
    run -0 cmp "$SRC_DATA$F" "$DST_DATA$F"
}

@test "an HTTP pull copy from a file URL fails with 400" {
    run -0 copy pull "file:///etc/hosts" "$DST_HTTPS$F"
    assert_output 400
}

@test "an HTTP push copy to a root URL fails with 400" {
    run -0 copy push "$SRC_HTTPS$F" "$DST_URL/$F"
    assert_output 400
}

@test "an HTTP pull copy with a scitag from an unreachable address fails with 500" {
    XRD_CONNECTIONRETRY=0 run -0 copy pull "https://255.255.255.255//$F" "$PMARK_HTTPS$F" -H 'scitag: 0'
    assert_output 500
}

@test "an HTTP pull copy to the server with packet marking writes the content of the source" {
    copy pull "$SRC_HTTPS$F" "$PMARK_HTTPS$F" -H 'scitag: 0'
    run -0 cmp "$SRC_DATA$F" "$PMARK_DATA$F"
}

@test "the server with packet marking runs with libcheckclose" {
    [[ -r /proc/self/maps ]] || skip 'no /proc'
    run -0 grep -q libcheckclose "/proc/$(<"$BATS_FILE_TMPDIR/pmark/xrootd.pid")/maps"
}

@test "the server with libcheckclose keeps running after the copies" {
    copy pull "$SRC_HTTPS$F" "$PMARK_HTTPS$F" -H 'scitag: 0'
    run -0 is_running "$(<"$BATS_FILE_TMPDIR/pmark/xrootd.pid")"
}

@test "an HTTP copy to the destination with both a Source and a Destination fails with 400" {
    run -0 curl --silent -L -X COPY -o body.txt -w '%{http_code}' --cacert "$CREDENTIALS/ca.pem" \
        -H "Authorization: Bearer $TOKEN" -H "TransferHeaderAuthorization: Bearer $TOKEN" \
        -H "Source: $SRC_HTTPS$F" -H "Destination: $DST_HTTPS$F" "$DST_HTTPS$F"
    assert_output 400
}

@test "an HTTP copy to the source with both a Source and a Destination fails with 400" {
    run -0 curl --silent -L -X COPY -o body.txt -w '%{http_code}' --cacert "$CREDENTIALS/ca.pem" \
        -H "Authorization: Bearer $TOKEN" -H "TransferHeaderAuthorization: Bearer $TOKEN" \
        -H "Source: $SRC_HTTPS$F" -H "Destination: $DST_HTTPS$F" "$SRC_HTTPS$F"
    assert_output 400
}

@test "the g-stream counts an HTTP copy with the status 202" {
    copy pull "$SRC_HTTPS$F" "$DST_HTTPS$F"
    run -0 wait_for 10 grep -qE '"HTTP_COPY_202":\{"count":[1-9]' "$MONITOR_RECORDS"
}

@test "a roots copy with the token in BEARER_TOKEN keeps the content" {
    BEARER_TOKEN=$TOKEN BEARER_TOKEN_FILE="$BATS_TEST_TMPDIR/missing" \
        xrdcp "roots://localhost:$SRC_PORT/$F" "roots://localhost:$DST_PORT/$F"
    run -0 cmp "$SRC_DATA$F" "$DST_DATA$F"
}
