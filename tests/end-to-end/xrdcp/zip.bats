#!/usr/bin/env bash

# ZIP archives: listing and extraction of members, and append. The tests
# share one server, with the archive /archive.zip on it, which has a stored
# member and a deflated member.

load ../setup/server.bash

setup_file() {
    require_commands python3

    start_server server server

    export MEMBERS="$BATS_FILE_TMPDIR"
    printf 'stored content\n%.0s' {1..100} > "$MEMBERS/stored.txt"
    printf 'deflated content\n%.0s' {1..1000} > "$MEMBERS/deflated.txt"

    python3 - "$MEMBERS" "$SERVER_DATA/archive.zip" <<'PY'
import sys, zipfile
src, archive = sys.argv[1:]
with zipfile.ZipFile(archive, 'w') as z:
    z.write(f'{src}/stored.txt', 'stored.txt', compress_type=zipfile.ZIP_STORED)
    z.write(f'{src}/deflated.txt', 'deflated.txt', compress_type=zipfile.ZIP_DEFLATED)
PY
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    ARCHIVE="/t${BATS_TEST_NUMBER}.zip"
}

@test "ls -Z lists the members of an archive" {
    run -0 xrdfs "$SERVER_URL" ls -Z /archive.zip
    assert_output "$(printf '/archive.zip/stored.txt\n/archive.zip/deflated.txt')"
}

@test "ls -l -Z shows the uncompressed size of a deflated member" {
    run -0 xrdfs "$SERVER_URL" ls -l -Z /archive.zip
    assert_line --regexp ' 17000 .* /archive.zip/deflated.txt$'
}

@test "--zip extracts a stored member unchanged" {
    xrdcp --zip stored.txt "$SERVER_URL//archive.zip" stored.txt
    run -0 cmp "$MEMBERS/stored.txt" stored.txt
}

@test "--zip extracts a deflated member unchanged" {
    xrdcp --zip deflated.txt "$SERVER_URL//archive.zip" deflated.txt
    run -0 cmp "$MEMBERS/deflated.txt" deflated.txt
}

@test "--zip of a member which does not exist fails" {
    run ! xrdcp --zip missing.txt "$SERVER_URL//archive.zip" missing.txt
    assert_output --partial 'Resource not found'
}

@test "--cksum zcrc32 verifies a deflated member" {
    run -0 xrdcp --cksum zcrc32 --zip deflated.txt "$SERVER_URL//archive.zip" deflated.txt
}

@test "--zip-append creates an archive with the file" {
    xrdcp --zip-append "$MEMBERS/stored.txt" "$SERVER_URL/$ARCHIVE"
    run -0 xrdfs "$SERVER_URL" ls -Z "$ARCHIVE"
    assert_output "$ARCHIVE/stored.txt"
}

@test "--zip-append adds a file to an existing archive" {
    xrdcp --zip-append "$MEMBERS/stored.txt" "$SERVER_URL/$ARCHIVE"
    xrdcp --zip-append "$MEMBERS/deflated.txt" "$SERVER_URL/$ARCHIVE"
    run -0 xrdfs "$SERVER_URL" ls -Z "$ARCHIVE"
    assert_line "$ARCHIVE/deflated.txt"
}

@test "a member which --zip-append added extracts unchanged" {
    xrdcp --zip-append "$MEMBERS/stored.txt" "$SERVER_URL/$ARCHIVE"
    xrdcp --zip-append "$MEMBERS/deflated.txt" "$SERVER_URL/$ARCHIVE"
    xrdcp --zip deflated.txt "$SERVER_URL/$ARCHIVE" deflated.txt
    run -0 cmp "$MEMBERS/deflated.txt" deflated.txt
}

@test "an archive which --zip-append wrote is valid for another ZIP reader" {
    xrdcp --zip-append "$MEMBERS/stored.txt" "$SERVER_URL/$ARCHIVE"
    xrdcp --zip-append "$MEMBERS/deflated.txt" "$SERVER_URL/$ARCHIVE"
    run -0 python3 -m zipfile -t "$SERVER_DATA$ARCHIVE"
}

# The status of one listing of the archive, for the concurrent listings below
list_archive() {
    xrdfs "$SERVER_URL" ls -Z /archive.zip > /dev/null 2>&1
    echo $?
}

# A response which came before the handler set its next step made a listing
# fail, or crash the client (fixed in XrdCl). One listing in a few hundred
# failed, thus run 2000 of them, 32 at a time.
@test "2000 concurrent listings of an archive all succeed" {
    export -f list_archive
    export SERVER_URL
    seq 2000 | xargs -P 32 -I{} bash -c list_archive > status.txt
    run -0 sort -u status.txt
    assert_output 0
}
