#!/usr/bin/env bash

# Redirects of chmod which a server returns: the server "refused" redirects
# to a port where no server listens, "unknown" to a host which does not
# resolve.

load ../setup/server.bash

setup_file() {
    # no server runs on NO_SERVER_PORT, thus the target refuses the connection
    printf 'xrootd.redirect localhost:%s chmod\n' "$NO_SERVER_PORT" > "$BATS_FILE_TMPDIR/refused.cfg"
}

setup() {
    start_server refused "$BATS_FILE_TMPDIR/refused.cfg"
}

# Start the server "unknown". It takes its target only when the resolver
# reports an unknown name (EAI_NONAME). Without a name server, for example in
# the network sandbox of Portage, the resolver reports a temporary failure
# (EAI_AGAIN), and the server refuses its configuration.
start_unknown() {
    python3 -c '
import socket, sys
try:
    socket.getaddrinfo("nosuchhost.invalid.", 1094)
except socket.gaierror as e:
    sys.exit(e.errno != socket.EAI_NONAME)
sys.exit(1)' || skip 'the resolver does not report nosuchhost.invalid. as an unknown name'
    start_server unknown redirect-unknown.cfg
}

teardown() {
    stop_servers
}

@test "a chmod which a server redirects to a port without a server fails with status 51" {
    XRD_CONNECTIONRETRY=0 run -51 xrdfs "$REFUSED_URL" chmod /nofile rwx------
}

@test "a chmod which a server redirects to a port without a server reports a connection error" {
    XRD_CONNECTIONRETRY=0 run ! xrdfs "$REFUSED_URL" chmod /nofile rwx------
    assert_output --partial '[FATAL] Connection error'
}

@test "a chmod which a server redirects to a host which does not resolve fails with status 51" {
    start_unknown
    XRD_CONNECTIONRETRY=0 run -51 xrdfs "$UNKNOWN_URL" chmod /nofile rwx------
}

@test "a chmod which a server redirects to a host which does not resolve reports an invalid address" {
    start_unknown
    XRD_CONNECTIONRETRY=0 run ! xrdfs "$UNKNOWN_URL" chmod /nofile rwx------
    assert_output --partial '[FATAL] Invalid address'
}
