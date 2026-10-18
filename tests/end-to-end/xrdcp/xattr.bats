#!/usr/bin/env bash

# Extended attributes in copies. The tests share one server.

load ../setup/server.bash

setup_file() {
    require_commands setfattr getfattr
    start_server server server
    require_xattr "$SERVER_DATA"
    require_xattr "$BATS_FILE_TMPDIR"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    TARGET="/t${BATS_TEST_NUMBER}.dat"
    echo 'content' > local.dat
    setfattr -n user.color -v blue local.dat
}

@test "upload with --xattr copies a user attribute" {
    xrdcp --xattr local.dat "$SERVER_URL/$TARGET"
    run -0 xrdfs "$SERVER_URL" xattr "$TARGET" list
    assert_line 'color="blue"'
}

@test "upload with XRD_PRESERVEXATTRS=1 copies a user attribute" {
    XRD_PRESERVEXATTRS=1 xrdcp local.dat "$SERVER_URL/$TARGET"
    run -0 xrdfs "$SERVER_URL" xattr "$TARGET" list
    assert_line 'color="blue"'
}

@test "upload without --xattr copies no attribute" {
    xrdcp local.dat "$SERVER_URL/$TARGET"
    run -0 xrdfs "$SERVER_URL" xattr "$TARGET" list
    refute_output --partial 'color'
}

@test "download with --xattr sets a user attribute" {
    xrdcp local.dat "$SERVER_URL/$TARGET"
    xrdfs "$SERVER_URL" xattr "$TARGET" set shape=round
    xrdcp --xattr "$SERVER_URL/$TARGET" copy.dat
    run -0 getfattr -n user.shape --only-values copy.dat
    assert_output 'round'
}

@test "local copy with --xattr keeps a user attribute" {
    xrdcp --xattr local.dat copy.dat
    run -0 getfattr -n user.color --only-values copy.dat
    assert_output 'blue'
}
