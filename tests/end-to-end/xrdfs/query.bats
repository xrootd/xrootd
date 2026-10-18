#!/usr/bin/env bash

# Queries which do not change the server. The tests share one server.

load ../setup/server.bash

setup_file() {
    start_server server server
}

teardown_file() {
    stop_servers
}

@test "the server version is the client version" {
    run -0 xrdfs "$SERVER_URL" query config version
    assert_output "$(xrdcp --version 2>&1)"
}

@test "query config sitename reports the server name" {
    run -0 xrdfs "$SERVER_URL" query config sitename
    assert_output 'server'
}

@test "query config role reports a data server" {
    run -0 xrdfs "$SERVER_URL" query config role
    assert_output 'server'
}

@test "query config chksum lists the configured checksums" {
    run -0 xrdfs "$SERVER_URL" query config chksum
    assert_output '0:adler32,1:crc32c,2:zcrc32,3:md5'
}

@test "query config cms reports no cluster" {
    run -0 xrdfs "$SERVER_URL" query config cms
    assert_output 'none|'
}

@test "query config tpc succeeds" {
    run -0 xrdfs "$SERVER_URL" query config tpc
}

@test "query config bind_max reports a number" {
    run -0 xrdfs "$SERVER_URL" query config bind_max
    assert_output --regexp '^[0-9]+$'
}

@test "query config pio_max reports a number" {
    run -0 xrdfs "$SERVER_URL" query config pio_max
    assert_output --regexp '^[0-9]+$'
}

@test "query config readv_ior_max reports a number" {
    run -0 xrdfs "$SERVER_URL" query config readv_ior_max
    assert_output --regexp '^[0-9]+$'
}

@test "query config readv_iov_max reports a number" {
    run -0 xrdfs "$SERVER_URL" query config readv_iov_max
    assert_output --regexp '^[0-9]+$'
}

@test "query config window reports a number" {
    run -0 xrdfs "$SERVER_URL" query config window
    assert_output --regexp '^[0-9]+$'
}

@test "query config wan_port succeeds" {
    run -0 xrdfs "$SERVER_URL" query config wan_port
}

@test "query config wan_window succeeds" {
    run -0 xrdfs "$SERVER_URL" query config wan_window
}

@test "stat of the root reports a directory" {
    run -0 xrdfs "$SERVER_URL" stat /
    assert_output --partial 'IsDir'
}

@test "statvfs reports one node with space" {
    run -0 xrdfs "$SERVER_URL" statvfs /
    assert_output --regexp 'Nodes with RW space: +1'
}

@test "spaceinfo reports the total space" {
    run -0 xrdfs "$SERVER_URL" spaceinfo /
    assert_output --regexp 'Total: +[0-9]+'
}
