#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

PORT=11968

setup() {
    launch_xrootd cks-realtime.cfg cks-realtime
    sleep 0.5
}

teardown() {
    kill_pid_files 2>/dev/null || true
}

bats::on_failure() {
    print_log_files
}

# The helper writes the local file to the server out of order: the first
# half block, then everything past the first block, then the hole. The
# server must combine the segment digests while the file is written, and
# must arrive at the same value the on-demand suite expects.

check() {
    local digest=$1 expected=$2
    run -0 xrdckstest real-time "${digest}" "${expected}" \
        "${BATS_TEST_DIRNAME}/XrdCksTestFile" \
        "root://localhost:${PORT}//TestFile.${digest}"
}

@test "real-time adler32 digest" {
    check adler32 64fce8a2
}

@test "real-time crc32 digest" {
    check crc32 55bd9dea
}

@test "real-time crc32c digest" {
    check crc32c cf3aa257
}

@test "real-time md5 digest" {
    check md5 b7018d4b11d10edbdba4a240e71d4976
}

# cks.type=default must select adler32, the first digest named on the
# xrootd.chksum directive.

@test "real-time default digest resolves to adler32" {
    check default 64fce8a2
}

# "ofs.cksrt auto default" resolves the same way. The effective
# configuration the server prints at startup is the only place this
# resolution is observable, since a wrong choice still yields the
# correct value through an on-demand recomputation.

@test "cksrt auto default resolves to adler32" {
    run -0 grep -E 'ofs\.cksrt +auto adler32 chkcgi' \
        "${BATS_TEST_TMPDIR}/cks-realtime.log"
}
