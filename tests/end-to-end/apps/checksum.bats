#!/usr/bin/env bash

# The checksum tools xrdadler32 and xrdcrc32c, for local files, stdin and
# root:// URLs. The tests share one server.

load ../setup/server.bash

setup_file() {
    start_server server server
    printf 'abcdefghijklmnopqrstuvw987' > "$SERVER_DATA/alphabet.txt"
}

teardown_file() {
    stop_servers
}

# The time of the last change of the file, in seconds
mtime() {
    stat -c %Y "$1" 2>/dev/null || stat -f %m "$1"
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    printf 'abcdefghijklmnopqrstuvw987' > alphabet.txt
    ADLER32=$(xrdadler32 < alphabet.txt | cut -d' ' -f1)
}

#
# xrdadler32
#

@test "xrdadler32 of stdin prints the checksum and a dash" {
    run -0 xrdadler32 < alphabet.txt
    assert_output --regexp '^[0-9a-f]{8} -$'
}

@test "xrdadler32 of a local file prints its checksum and path" {
    run -0 xrdadler32 alphabet.txt
    assert_output "$ADLER32 alphabet.txt"
}

@test "xrdadler32 of a local file stores the checksum in the attribute of XrdCks" {
    require_commands getfattr
    require_xattr "$BATS_TEST_TMPDIR"
    xrdadler32 alphabet.txt
    run -0 getfattr -n user.XrdCks.adler32 alphabet.txt
}

# The legacy attribute holds <checksum>:<time of the file>
@test "xrdadler32 prints the checksum of a legacy attribute for the time of the file" {
    require_commands setfattr
    require_xattr "$BATS_TEST_TMPDIR"
    setfattr -n user.checksum.adler32 -v "deadbeef:$(mtime alphabet.txt)" alphabet.txt
    run -0 xrdadler32 alphabet.txt
    assert_output 'deadbeef alphabet.txt'
}

@test "xrdadler32 replaces a legacy attribute by the attribute of XrdCks" {
    require_commands getfattr setfattr
    require_xattr "$BATS_TEST_TMPDIR"
    setfattr -n user.checksum.adler32 -v "deadbeef:$(mtime alphabet.txt)" alphabet.txt
    xrdadler32 alphabet.txt
    run ! getfattr -n user.checksum.adler32 alphabet.txt
}

@test "xrdadler32 ignores a stored checksum of an older time of the file" {
    require_commands setfattr
    require_xattr "$BATS_TEST_TMPDIR"
    setfattr -n user.checksum.adler32 -v 'deadbeef:1' alphabet.txt
    run -0 xrdadler32 alphabet.txt
    assert_output "$ADLER32 alphabet.txt"
}

@test "xrdadler32 of a missing file fails" {
    run -1 xrdadler32 missing.txt
    assert_output --partial 'Error opening missing.txt'
}

@test "xrdadler32 -h prints the usage" {
    run -0 xrdadler32 -h
    assert_output --partial 'Usage:'
}

@test "xrdadler32 of a root URL prints the checksum of the server" {
    run -0 xrdadler32 "$SERVER_URL//alphabet.txt"
    assert_output "$ADLER32 $SERVER_URL//alphabet.txt"
}

@test "xrdadler32 of a missing root URL fails" {
    run -1 xrdadler32 "$SERVER_URL//missing.txt"
    assert_output --partial 'Error_accessing'
}

#
# xrdcrc32c
#

@test "xrdcrc32c of a local file prints its checksum and path" {
    run -0 xrdcrc32c alphabet.txt
    assert_output --regexp '^[0-9a-f]{8} alphabet.txt$'
}

@test "xrdcrc32c -s prints only the checksum" {
    run -0 xrdcrc32c -s alphabet.txt
    assert_output --regexp '^[0-9a-f]{8}$'
}

@test "xrdcrc32c of stdin prints the same checksum as of the file" {
    run -0 bats_pipe xrdcrc32c < alphabet.txt \| cut -d' ' -f1
    assert_output "$(xrdcrc32c -s alphabet.txt)"
}

@test "xrdcrc32c -n prints no newline" {
    run -0 bats_pipe xrdcrc32c -n -s alphabet.txt \| wc -l
    assert_output --regexp '^ *0$'
}

@test "xrdcrc32c -x prints the checksum without leading zeros" {
    run -0 xrdcrc32c -x -s alphabet.txt
    assert_output "$(printf '%x' "0x$(xrdcrc32c -s alphabet.txt)")"
}

@test "xrdcrc32c -h prints the usage" {
    run xrdcrc32c -h
    assert_output --partial 'Usage: xrdcrc32c'
}

@test "xrdcrc32c of a missing file fails" {
    run ! xrdcrc32c missing.txt
}
