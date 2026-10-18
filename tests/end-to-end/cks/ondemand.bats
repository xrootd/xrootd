#!/usr/bin/env bash

load ../setup/server.bash

setup_file() {
    start_server server server ondemand.cfg
}

teardown_file() {
    stop_servers
}

# The helper writes the local file to the server out of order, then asks
# the server for the digest. On demand, the server reads the file back.

check() {
    local digest=$1 expected=$2
    run -0 xrdckstest on-demand "${digest}" "${expected}" \
        "${BATS_TEST_DIRNAME}/XrdCksTestFile" \
        "$SERVER_URL//TestFile.${digest}"
}

@test "on-demand adler32 digest" {
    check adler32 64fce8a2
}

@test "on-demand crc32 digest" {
    check crc32 55bd9dea
}

@test "on-demand crc32c digest" {
    check crc32c cf3aa257
}

@test "on-demand md5 digest" {
    check md5 b7018d4b11d10edbdba4a240e71d4976
}

# cks.type=default must select adler32, the first digest named on the
# xrootd.chksum directive.

@test "on-demand default digest resolves to adler32" {
    check default 64fce8a2
}
