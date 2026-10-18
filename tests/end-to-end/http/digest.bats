#!/usr/bin/env bash

# Checksums in HTTP headers: Want-Digest (RFC 3230) and Want-Repr-Digest
# (RFC 9530). The tests share one server, with a file of 26 bytes.

load ../setup/server.bash

readonly MD5_B64='mRykpCtRV62NckS3pmYroQ=='
readonly ADLER_B64='jwQKXQ=='

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    start_server server server http server.cfg
    printf 'abcdefghijklmnopqrstuvw987' > "$SERVER_DATA/alphabet.txt"
    ADLER32=$(checksum adler32 "$SERVER_DATA/alphabet.txt")
    CRC32C=$(checksum crc32c "$SERVER_DATA/alphabet.txt")
    export ADLER32 CRC32C
}

teardown_file() {
    stop_servers
}

# The response headers of a HEAD request with the given headers, without CR
head_with() {
    curl --silent -I "$@" "$SERVER_HTTP/alphabet.txt" | tr -d '\r'
}

# The response headers of a GET request with the given headers, without CR
get_with() {
    curl --silent -D - -o /dev/null "$@" "$SERVER_HTTP/alphabet.txt" | tr -d '\r'
}

@test "HEAD with Want-Digest adler32 succeeds" {
    run -0 head_with -H 'Want-Digest: adler32'
    assert_line --partial '200 OK'
}

@test "HEAD with Want-Digest adler32 returns the adler32 digest" {
    run -0 head_with -H 'Want-Digest: adler32'
    assert_line "Digest: adler32=$ADLER32"
}

@test "HEAD returns the content length" {
    run -0 head_with -H 'Want-Digest: adler32'
    assert_line 'Content-Length: 26'
}

@test "HEAD with Want-Digest adler returns the adler digest" {
    run -0 head_with -H 'Want-Digest: adler'
    assert_line "Digest: adler=$ADLER32"
}

@test "HEAD with Want-Digest crc32c returns the crc32c digest" {
    run -0 head_with -H 'Want-Digest: crc32c'
    assert_line "Digest: crc32c=$CRC32C"
}

@test "HEAD with Want-Digest md5 returns the md5 digest in base64" {
    run -0 head_with -H 'Want-Digest: md5'
    assert_line "Digest: md5=$MD5_B64"
}

@test "HEAD with an unsupported digest first returns the next supported one" {
    run -0 head_with -H 'Want-Digest: NotSupported, adler32, crc32c'
    assert_line "Digest: adler32=$ADLER32"
}

@test "HEAD with Want-Repr-Digest adler returns the Repr-Digest" {
    run -0 head_with -H 'Want-Repr-Digest: adler=1'
    assert_line "Repr-Digest: adler=:$ADLER_B64:"
}

@test "HEAD with Want-Repr-Digest of two digests returns the preferred one" {
    run -0 head_with -H 'Want-Repr-Digest: adler=1,md5=5'
    assert_line "Repr-Digest: md5=:$MD5_B64:"
}

@test "HEAD with both Want-Digest and Want-Repr-Digest returns the Digest" {
    run -0 head_with -H 'Want-Digest: adler32' -H 'Want-Repr-Digest: md5=1'
    assert_line "Digest: adler32=$ADLER32"
}

@test "HEAD with a malformed Want-Repr-Digest returns no digest" {
    run -0 head_with -H 'Want-Repr-Digest: adler32'
    refute_output --partial 'Digest'
}

@test "GET with Want-Repr-Digest returns the Repr-Digest" {
    run -0 get_with -H 'Want-Repr-Digest: adler=1'
    assert_line "Repr-Digest: adler=:$ADLER_B64:"
}

@test "GET with both Want-Digest and Want-Repr-Digest returns the Digest" {
    run -0 get_with -H 'Want-Digest: adler32' -H 'Want-Repr-Digest: md5=1'
    assert_line "Digest: adler32=$ADLER32"
}
