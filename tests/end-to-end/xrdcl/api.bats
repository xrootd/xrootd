#!/usr/bin/env bash

# XrdCl API calls which no command-line tool makes, through the helper
# xrdcl-api. The tests share one server, and each test has its own file of 20
# bytes on it.

load ../setup/server.bash

# BATS stops a test and its processes after this time, thus a client which
# hangs fails its test
# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=60

readonly CONTENT='0123456789abcdefghij'

setup_file() {
    start_server server server
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    FILE="/t${BATS_TEST_NUMBER}.dat"
    printf '%s' "$CONTENT" > "$SERVER_DATA$FILE"
}

# Print a block of 4 KiB of the given character
block() {
    printf "$1%.0s" {1..4096}
}

# Skip the test when the server does not support the operation (exit 3)
skip_unsupported() {
    (( status != 3 )) || skip "the server does not support it: $output"
}

@test "File::Read returns the data at an offset" {
    run -0 xrdcl-api read "$SERVER_URL/$FILE" 5 5
    assert_output '56789'
}

@test "File::Read past the end of the file returns the rest of the file" {
    run -0 xrdcl-api read "$SERVER_URL/$FILE" 15 100
    assert_output 'fghij'
}

@test "File::PgRead returns the data at an offset" {
    run -0 xrdcl-api pgread "$SERVER_URL/$FILE" 2 6
    assert_output '234567'
}

@test "File::PgRead of several pages returns the data" {
    make_random_file "$SERVER_DATA$FILE" 12345
    xrdcl-api pgread "$SERVER_URL/$FILE" 0 12345 > copy.dat
    run -0 cmp "$SERVER_DATA$FILE" copy.dat
}

@test "File::VectorRead returns the data of each chunk" {
    run -0 xrdcl-api readv "$SERVER_URL/$FILE" 0:2 10:3
    assert_output '01abc'
}

@test "File::Stat reports the size of the file" {
    run -0 xrdcl-api stat "$SERVER_URL/$FILE"
    assert_output '20'
}

@test "File::Stat with force reports the size of the file" {
    run -0 xrdcl-api stat "$SERVER_URL/$FILE" force
    assert_output '20'
}

@test "File::Write replaces the data at an offset" {
    printf 'XY' | xrdcl-api write "$SERVER_URL/$FILE" 3
    assert_equal "$(<"$SERVER_DATA$FILE")" '012XY56789abcdefghij'
}

@test "File::PgWrite replaces the data at an offset" {
    printf 'XY' | xrdcl-api pgwrite "$SERVER_URL/$FILE" 3
    assert_equal "$(<"$SERVER_DATA$FILE")" '012XY56789abcdefghij'
}

@test "File::PgWrite of several pages writes the data" {
    make_random_file local.dat 12345
    xrdcl-api pgwrite "$SERVER_URL/$FILE" 0 < local.dat
    run -0 cmp local.dat "$SERVER_DATA$FILE"
}

@test "File::WriteV writes the buffers one after the other" {
    printf 'aa' > a
    printf 'bbb' > b
    xrdcl-api writev "$SERVER_URL/$FILE" 10 a b
    assert_equal "$(<"$SERVER_DATA$FILE")" '0123456789aabbbfghij'
}

@test "File::VectorWrite writes each chunk at its offset" {
    printf 'aa' > a
    printf 'bbb' > b
    xrdcl-api vwrite "$SERVER_URL/$FILE" 0:a 17:b
    assert_equal "$(<"$SERVER_DATA$FILE")" 'aa23456789abcdefgbbb'
}

@test "File::Truncate sets the size of the file" {
    xrdcl-api truncate "$SERVER_URL/$FILE" 8
    assert_equal "$(<"$SERVER_DATA$FILE")" '01234567'
}

@test "a checkpoint which rolls back keeps the old data" {
    printf 'ZZZZ' | xrdcl-api checkpoint "$SERVER_URL/$FILE" 0 rollback
    assert_equal "$(<"$SERVER_DATA$FILE")" "$CONTENT"
}

@test "a checkpoint which commits applies the write" {
    printf 'ZZZZ' | xrdcl-api checkpoint "$SERVER_URL/$FILE" 0 commit
    assert_equal "$(<"$SERVER_DATA$FILE")" 'ZZZZ456789abcdefghij'
}

@test "File::SetXAttr sets an attribute which File::GetXAttr returns" {
    require_xattr "$SERVER_DATA"
    xrdcl-api xattr "$SERVER_URL/$FILE" set color blue
    run -0 xrdcl-api xattr "$SERVER_URL/$FILE" get color
    assert_output 'blue'
}

@test "File::ListXAttr lists an attribute" {
    require_xattr "$SERVER_DATA"
    xrdcl-api xattr "$SERVER_URL/$FILE" set color blue
    run -0 xrdcl-api xattr "$SERVER_URL/$FILE" list
    assert_line 'color=blue'
}

@test "File::DelXAttr removes an attribute" {
    require_xattr "$SERVER_DATA"
    xrdcl-api xattr "$SERVER_URL/$FILE" set color blue
    xrdcl-api xattr "$SERVER_URL/$FILE" del color
    run -1 xrdcl-api xattr "$SERVER_URL/$FILE" get color
}

@test "File::Clone copies the ranges of the source" {
    # blocks of 4 KiB, as a clone of file ranges needs aligned ranges
    block A > a; block B > b; block C > c
    cat a b c > "$SERVER_DATA$FILE"
    run xrdcl-api clone "$SERVER_URL/$FILE" "$SERVER_URL/$FILE.clone" 0:8192:4096 4096:0:4096
    skip_unsupported
    assert_success
    cat c a > expected
    run -0 cmp expected "$SERVER_DATA$FILE.clone"
}

@test "a file opened as a duplicate has the content of the source" {
    run xrdcl-api dup "$SERVER_URL/$FILE" "$SERVER_URL/$FILE.dup"
    skip_unsupported
    assert_success
    assert_equal "$(<"$SERVER_DATA$FILE.dup")" "$CONTENT"
}

@test "FileSystem::Ping succeeds" {
    run -0 xrdcl-api ping "$SERVER_URL"
}

@test "FileSystem::Protocol reports the protocol version" {
    run -0 xrdcl-api protocol "$SERVER_URL"
    assert_output --partial 'version=520 '
}

@test "FileSystem::Protocol reports a data server" {
    run -0 xrdcl-api protocol "$SERVER_URL"
    # kXR_isServer is bit 0 of the host information
    assert_output --regexp 'hostinfo=[0-9a-f]*[13579bdf]$'
}

@test "FileSystem::SendInfo returns an identifier of 4 bytes" {
    run -0 xrdcl-api sendinfo "$SERVER_URL" 'end-to-end test'
    assert_output '4'
}

@test "a chunked FileSystem::DirList lists all entries" {
    mkdir "$SERVER_DATA/d${BATS_TEST_NUMBER}"
    for i in $(seq 3000); do : > "$SERVER_DATA/d${BATS_TEST_NUMBER}/file-with-a-long-name-$i"; done
    run -0 --separate-stderr xrdcl-api dirlist "$SERVER_URL" "/d${BATS_TEST_NUMBER}" chunked
    assert_equal "${#lines[@]}" 3000
}

@test "a chunked FileSystem::DirList of many entries has several responses" {
    mkdir "$SERVER_DATA/d${BATS_TEST_NUMBER}"
    for i in $(seq 3000); do : > "$SERVER_DATA/d${BATS_TEST_NUMBER}/file-with-a-long-name-$i"; done
    run -0 --separate-stderr xrdcl-api dirlist "$SERVER_URL" "/d${BATS_TEST_NUMBER}" chunked
    assert_regex "$stderr" '^responses: ([2-9]|[1-9][0-9]+)$'
}

@test "a chunked FileSystem::DirList of an empty directory lists nothing" {
    mkdir "$SERVER_DATA/d${BATS_TEST_NUMBER}"
    run -0 --separate-stderr xrdcl-api dirlist "$SERVER_URL" "/d${BATS_TEST_NUMBER}" chunked
    assert_output ''
}

@test "FileSystem::DirList of a missing directory fails" {
    run ! xrdcl-api dirlist "$SERVER_URL" "/d${BATS_TEST_NUMBER}"
}

# The server opens the directory without a stat before; a failed open must
# not leave a directory object without a name behind (fixed in XrdOfs)
@test "the server keeps running after a DirList of a missing directory" {
    xrdcl-api dirlist "$SERVER_URL" "/d${BATS_TEST_NUMBER}" || true
    run -0 server_is_up "$SERVER_URL"
}

@test "concurrent reads of one File object return the whole file" {
    make_random_file "$SERVER_DATA$FILE" 3000001
    xrdcl-api threads "$SERVER_URL/$FILE" 20 > copy.dat
    run -0 cmp "$SERVER_DATA$FILE" copy.dat
}

@test "concurrent reads of one File object over 4 substreams return the whole file" {
    make_random_file "$SERVER_DATA$FILE" 3000001
    XRD_SUBSTREAMSPERCHANNEL=4 xrdcl-api threads "$SERVER_URL/$FILE" 20 > copy.dat
    run -0 cmp "$SERVER_DATA$FILE" copy.dat
}
