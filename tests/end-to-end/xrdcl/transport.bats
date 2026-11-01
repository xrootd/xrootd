#!/usr/bin/env bash

# The transport classes of XrdCl: Socket and the built-in Poller. They carry
# the XRootD handshake to a server and its response back, and random data to
# and from a peer in xrdcl-api, which compares the checksums of the data of
# each direction. Each test has its own server.

load ../setup/server.bash

# BATS stops a test and its processes after this time, thus a client which
# hangs fails its test
# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=60

setup() {
    start_server
}

teardown() {
    stop_servers
}

@test "Socket sends the handshake and reads the response of the server" {
    require_ipv6_socket
    run -0 xrdcl-api socket localhost "$SERVER_PORT"
    assert_output 'msglen=8 protover=520 type=1'
}

@test "a socket of the IPv6 family connects to an IPv4 address" {
    require_ipv6_socket
    run -0 xrdcl-api socket 127.0.0.1 "$SERVER_PORT"
    assert_output 'msglen=8 protover=520 type=1'
}

@test "a socket of the IPv4 family sends the handshake and reads the response" {
    run -0 xrdcl-api socket 127.0.0.1 "$SERVER_PORT" ipv4
    assert_output 'msglen=8 protover=520 type=1'
}

@test "the Poller delivers the response of the server on each of 3 sockets" {
    require_ipv6_socket
    run -0 xrdcl-api poller localhost "$SERVER_PORT" 3
    assert_equal "$(grep -c '^msglen=8 protover=520 type=1$' <<< "$output")" 3
}

@test "the Poller delivers the responses on 50 sockets" {
    require_ipv6_socket
    run -0 xrdcl-api poller localhost "$SERVER_PORT" 50
    assert_equal "$(grep -c '^msglen=8 protover=520 type=1$' <<< "$output")" 50
}

@test "Socket reads and writes 100 packets of random data in both directions" {
    run -0 xrdcl-api socket-transfer 100
    assert_line --regexp '^packets=100 received=[0-9]+ sent=[0-9]+ checksums match$'
}

@test "Socket is disconnected before the connection and after the close" {
    run -0 xrdcl-api socket-transfer 1
    assert_line 'status: disconnected connected disconnected'
}

@test "the Poller delivers all data of the peer on each of 3 sockets" {
    run -0 xrdcl-api poller-transfer 3
    assert_output --regexp '^sockets=3 bytes=[0-9]+ events=[0-9]+ checksums match$'
}

@test "the Poller delivers all data of the peer on each of 20 sockets" {
    run -0 xrdcl-api poller-transfer 20
    assert_output --regexp '^sockets=20 bytes=[0-9]+ events=[0-9]+ checksums match$'
}

# The peer pauses after each packet, thus the data of a socket comes in parts
@test "the Poller delivers the data of a socket over many read events" {
    run -0 xrdcl-api poller-transfer 3
    [[ "$output" =~ events=([0-9]+) ]]
    assert [ "${BASH_REMATCH[1]}" -gt 3 ]
}
