#!/usr/bin/env bash

# The transport classes of XrdCl: Socket and the built-in Poller, which carry
# the XRootD handshake to a server and its response back. The tests share one
# server.

load ../setup/server.bash

# BATS stops a test and its processes after this time, thus a client which
# hangs fails its test
# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=60

setup_file() {
    start_server server server
}

teardown_file() {
    stop_servers
}

@test "Socket sends the handshake and reads the response of the server" {
    run -0 xrdcl-api socket localhost "$SERVER_PORT"
    assert_output 'msglen=8 protover=520 type=1'
}

@test "a socket of the IPv6 family connects to an IPv4 address" {
    run -0 xrdcl-api socket 127.0.0.1 "$SERVER_PORT"
    assert_output 'msglen=8 protover=520 type=1'
}

@test "a socket of the IPv4 family sends the handshake and reads the response" {
    run -0 xrdcl-api socket 127.0.0.1 "$SERVER_PORT" ipv4
    assert_output 'msglen=8 protover=520 type=1'
}

@test "the Poller delivers the response of the server on each of 3 sockets" {
    run -0 xrdcl-api poller localhost "$SERVER_PORT" 3
    assert_equal "$(grep -c '^msglen=8 protover=520 type=1$' <<< "$output")" 3
}

@test "the Poller delivers the responses on 50 sockets" {
    run -0 xrdcl-api poller localhost "$SERVER_PORT" 50
    assert_equal "$(grep -c '^msglen=8 protover=520 type=1$' <<< "$output")" 50
}
