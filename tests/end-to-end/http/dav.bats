#!/usr/bin/env bash

# xrdfs and xrdcp over dav://, with the XrdClHttp plugin. The tests share one
# server.

load ../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"

setup_file() {
    require_plugin libXrdHttp
    require_plugin libXrdClHttp
    start_server server server http server.cfg
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    DAV="dav://localhost:$SERVER_PORT"
    D="/t${BATS_TEST_NUMBER}"
}

@test "a query over dav is not implemented" {
    run ! xrdfs "$DAV" query config version
    assert_output --partial 'not implemented'
}

@test "mkdir -p without a path fails over dav" {
    run ! xrdfs "$DAV" mkdir -p
    assert_output --partial 'A path is required.'
}

@test "mkdir of a relative path fails over dav" {
    run ! xrdfs "$DAV" mkdir foo
    assert_output --partial "Creating relative path 'foo' is disallowed."
}

@test "rmdir of a relative path fails over dav" {
    run ! xrdfs "$DAV" rmdir foo
    assert_output --partial "Removing relative path 'foo' is disallowed."
}

@test "rm of a relative path fails over dav" {
    run ! xrdfs "$DAV" rm foo
    assert_output --partial "Removing relative path 'foo' is disallowed."
}

@test "ls of a relative path fails over dav" {
    run ! xrdfs "$DAV" ls foo
    assert_output --partial "Listing relative path 'foo' is disallowed."
}

@test "mkdir of a path to the parent fails over dav" {
    run ! xrdfs "$DAV" mkdir ../foo
    assert_output --partial "Creating relative path '../foo' is disallowed."
}

@test "interactive mkdir of a path above the root fails over dav" {
    run bats_pipe printf 'mkdir ../foo\nexit\n' \| xrdfs "$DAV"
    assert_output --partial "Path '../foo' escapes above root."
}

@test "mkdir -p creates a directory over dav" {
    xrdfs "$DAV" mkdir -p "$D/sub"
    run -0 test -d "$SERVER_DATA$D/sub"
}

@test "rmdir removes a directory over dav" {
    mkdir "$SERVER_DATA$D"
    xrdfs "$DAV" rmdir "$D"
    run ! test -e "$SERVER_DATA$D"
}

@test "an upload over dav keeps the content" {
    make_random_file local.dat 1000003
    xrdcp local.dat "$DAV/$D.dat"
    run -0 cmp local.dat "$SERVER_DATA$D.dat"
}

@test "a download over dav keeps the adler32 checksum" {
    make_random_file "$SERVER_DATA$D.dat" 1000003
    xrdcp "$DAV/$D.dat" copy.dat
    assert_equal "$(checksum adler32 copy.dat)" "$(checksum adler32 "$SERVER_DATA$D.dat")"
}

@test "a download over dav keeps the crc32c checksum" {
    make_random_file "$SERVER_DATA$D.dat" 1000003
    xrdcp "$DAV/$D.dat" copy.dat
    assert_equal "$(checksum crc32c copy.dat)" "$(checksum crc32c "$SERVER_DATA$D.dat")"
}

@test "ls over dav lists an uploaded file" {
    mkdir "$SERVER_DATA$D"
    echo content > "$SERVER_DATA$D/file"
    run -0 xrdfs "$DAV" ls "$D"
    assert_output "$D/file"
}

@test "rm over dav removes a file" {
    echo content > "$SERVER_DATA$D.txt"
    xrdfs "$DAV" rm "$D.txt"
    run ! test -e "$SERVER_DATA$D.txt"
}
