#!/usr/bin/env bash

# Data integrity of uploads and downloads. Each test has its own server, with
# the file /ref.dat on it.

load ../setup/server.bash

# More 8 MiB chunks than xrdcp keeps in flight (4), and a partial last chunk
readonly SIZE=$(( 5 * 8 * 1024 * 1024 + 7 ))

setup_file() {
    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" "$SIZE"
}

setup() {
    start_server checksum.cfg

    xrdcp "$REF" "$SERVER_URL//ref.dat"

    cd "$BATS_TEST_TMPDIR"
}

teardown() {
    stop_servers
}

@test "upload of a file succeeds" {
    run -0 xrdcp "$REF" "$SERVER_URL//t${BATS_TEST_NUMBER}.dat"
}

@test "upload with the legacy option -np succeeds" {
    run -0 xrdcp -np "$REF" "$SERVER_URL//t${BATS_TEST_NUMBER}.dat"
}

@test "upload keeps the size of the file" {
    xrdcp "$REF" "$SERVER_URL//t${BATS_TEST_NUMBER}.dat"
    run -0 xrdfs "$SERVER_URL" stat "/t${BATS_TEST_NUMBER}.dat"
    assert_line --regexp "^Size: +$SIZE\$"
}

@test "download keeps the adler32 checksum" {
    local copy ref
    xrdcp "$SERVER_URL//ref.dat" copy.dat
    copy=$(checksum adler32 copy.dat)
    ref=$(checksum adler32 "$REF")
    assert_equal "$copy" "$ref"
}

@test "download keeps the crc32c checksum" {
    local copy ref
    xrdcp "$SERVER_URL//ref.dat" copy.dat
    copy=$(checksum crc32c copy.dat)
    ref=$(checksum crc32c "$REF")
    assert_equal "$copy" "$ref"
}

@test "download with --cksum adler32 matches the checksum of the server" {
    # the server stores checksums in extended attributes
    require_xattr "$SERVER_DATA"
    run -0 xrdcp --cksum adler32 "$SERVER_URL//ref.dat" copy.dat
}

@test "upload with --cksum crc32c matches the checksum of the server" {
    # the server stores checksums in extended attributes
    require_xattr "$SERVER_DATA"
    run -0 xrdcp --cksum crc32c "$REF" "$SERVER_URL//t${BATS_TEST_NUMBER}.dat"
}

@test "a small file with a partial last page is unchanged by a round trip" {
    # three full 4 KiB pages and a partial one in a single request
    make_random_file small.dat 12345
    xrdcp small.dat "$SERVER_URL//t${BATS_TEST_NUMBER}.dat"
    xrdcp "$SERVER_URL//t${BATS_TEST_NUMBER}.dat" copy.dat
    run -0 cmp small.dat copy.dat
}

@test "server adler32 checksum matches the local checksum" {
    # the server stores checksums in extended attributes
    require_xattr "$SERVER_DATA"
    run -0 xrdfs "$SERVER_URL" query checksum '/ref.dat?cks.type=adler32'
    assert_output "adler32 $(checksum adler32 "$REF")"
}

@test "server crc32c checksum matches the local checksum" {
    # the server stores checksums in extended attributes
    require_xattr "$SERVER_DATA"
    run -0 xrdfs "$SERVER_URL" query checksum '/ref.dat?cks.type=crc32c'
    assert_output "crc32c $(checksum crc32c "$REF")"
}
