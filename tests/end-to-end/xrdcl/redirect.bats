#!/usr/bin/env bash

# Redirects which the server returns. The fault injection of libXrdOssTests
# redirects an open of /cluster_redirfile1 to the local file
# /tmp/cluster_redirfile1_local, which does not exist.

load ../setup/server.bash

setup_file() {
    start_server server server
    echo 'content' > "$SERVER_DATA/cluster_redirfile1"
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
