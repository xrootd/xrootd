#!/usr/bin/env bash

# Redirects which the server returns. The fault injection of libXrdOssTests
# redirects an open of /cluster_redirfile1 to the local file
# /tmp/cluster_redirfile1_local, which does not exist. Two more servers
# redirect chmod: "refused" to a port where no server listens, "unknown" to
# a host which does not resolve.

load ../setup/server.bash

setup_file() {
    start_server server server
    echo 'content' > "$SERVER_DATA/cluster_redirfile1"

    # no server runs on NO_SERVER_PORT, thus the target refuses the connection
    printf 'xrootd.redirect localhost:%s chmod\n' "$NO_SERVER_PORT" > "$BATS_FILE_TMPDIR/refused.cfg"
    start_server refused server "$BATS_FILE_TMPDIR/refused.cfg"
    start_server unknown server redirect-unknown.cfg
}

teardown_file() {
    stop_servers
}

@test "a redirect to a local file which does not exist fails with status 54" {
    run -54 xrdcp "$SERVER_URL//cluster_redirfile1" /dev/null
}

@test "a redirect to a local file which does not exist reports the local error" {
    run ! xrdcp "$SERVER_URL//cluster_redirfile1" /dev/null
    assert_output --partial 'Local error: no such file or directory'
}

@test "a chmod which a server redirects to a port without a server fails with status 51" {
    XRD_CONNECTIONRETRY=0 run -51 xrdfs "$REFUSED_URL" chmod /nofile rwx------
}

@test "a chmod which a server redirects to a port without a server reports a connection error" {
    XRD_CONNECTIONRETRY=0 run ! xrdfs "$REFUSED_URL" chmod /nofile rwx------
    assert_output --partial '[FATAL] Connection error'
}

@test "a chmod which a server redirects to a host which does not resolve fails with status 51" {
    XRD_CONNECTIONRETRY=0 run -51 xrdfs "$UNKNOWN_URL" chmod /nofile rwx------
}

@test "a chmod which a server redirects to a host which does not resolve reports an invalid address" {
    XRD_CONNECTIONRETRY=0 run ! xrdfs "$UNKNOWN_URL" chmod /nofile rwx------
    assert_output --partial '[FATAL] Invalid address'
}
