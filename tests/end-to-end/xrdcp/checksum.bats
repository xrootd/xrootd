#!/usr/bin/env bash

# End-to-end checksums of xrdcp. The tests share one server, with the file
# /ref.dat on it.

load ../setup/server.bash

setup_file() {
    start_server server server

    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" 1000003
    cp "$REF" "$SERVER_DATA/ref.dat"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    TARGET="/t${BATS_TEST_NUMBER}.dat"
}

@test "download verifies the adler32 checksum" {
    run -0 xrdcp --cksum adler32 "$SERVER_URL//ref.dat" copy.dat
}

@test "download verifies the crc32c checksum" {
    run -0 xrdcp --cksum crc32c "$SERVER_URL//ref.dat" copy.dat
}

@test "download verifies the zcrc32 checksum" {
    run -0 xrdcp --cksum zcrc32 "$SERVER_URL//ref.dat" copy.dat
}

@test "download verifies the md5 checksum" {
    run -0 xrdcp --cksum md5 "$SERVER_URL//ref.dat" copy.dat
}

@test "download with --cksum auto verifies a checksum of the server" {
    run -0 xrdcp --cksum auto "$SERVER_URL//ref.dat" copy.dat
}

@test "upload verifies the adler32 checksum" {
    run -0 xrdcp --cksum adler32 "$REF" "$SERVER_URL/$TARGET"
}

@test "upload verifies the zcrc32 checksum" {
    run -0 xrdcp --cksum zcrc32 "$REF" "$SERVER_URL/$TARGET"
}

@test "upload verifies the md5 checksum" {
    run -0 xrdcp --cksum md5 "$REF" "$SERVER_URL/$TARGET"
}

@test "--cksum adler32:print prints the adler32 of the file" {
    run -0 xrdcp --cksum adler32:print "$SERVER_URL//ref.dat" copy.dat
    assert_output --partial "adler32: $(checksum adler32 "$REF") "
}

# The adler32 of the byte 0x01 is 00020002 (fixed in XrdCl)
@test "--cksum adler32:print prints the leading zeros of the checksum" {
    printf '\001' > one.dat
    run -0 xrdcp --cksum adler32:print one.dat copy.dat
    assert_output --partial 'adler32: 00020002 '
}

@test "--cksum md5:print prints the md5 of the file" {
    require_commands md5sum
    run -0 xrdcp --cksum md5:print "$SERVER_URL//ref.dat" copy.dat
    assert_output --partial "md5: $(md5sum < "$REF" | cut -d' ' -f1) "
}

@test "the server zcrc32 of an upload is the zcrc32 which xrdcp prints" {
    require_xattr "$SERVER_DATA"
    run -0 xrdcp --cksum zcrc32:print "$REF" "$SERVER_URL/$TARGET"
    local printed=${output#zcrc32: }
    run -0 xrdfs "$SERVER_URL" query checksum "$TARGET?cks.type=zcrc32"
    assert_output "zcrc32 ${printed%% *}"
}

@test "upload with a wrong expected checksum fails with a checksum error" {
    run ! xrdcp --cksum adler32:00000001 "$REF" "$SERVER_URL/$TARGET"
    assert_output --partial 'CheckSum error'
}

@test "upload with a wrong expected checksum keeps the target" {
    xrdcp --cksum adler32:00000001 "$REF" "$SERVER_URL/$TARGET" || true
    run -0 xrdfs "$SERVER_URL" stat "$TARGET"
}

@test "--rm-bad-cksum removes the target of a wrong checksum" {
    xrdcp --cksum adler32:00000001 --rm-bad-cksum "$REF" "$SERVER_URL/$TARGET" || true
    run ! xrdfs "$SERVER_URL" stat "$TARGET"
    assert_output --partial '[3011]'
}
