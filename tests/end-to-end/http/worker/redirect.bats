#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

export XRD_PLUGINCONFDIR="$BATS_TEST_DIRNAME"
export XRD_REQUESTTIMEOUT=15
export XRD_HTTPNUMTHREADS=1 XRD_HTTPDISABLEX509=1
export X509_USER_PROXY=/dev/null/x509
export no_proxy=127.0.0.1
export NO_PROXY=127.0.0.1
unset BEARER_TOKEN BEARER_TOKEN_FILE XrdSecPROTOCOL XRD_PLUGIN
unset XRD_HTTPHEADERS XRD_HTTPCLIENTCERTFILE XRD_HTTPCLIENTKEYFILE

setup() {
    cd "$BATS_TEST_TMPDIR" || return

    PORT=8397 launch_xrootd server.cfg target
    PORT=8396 launch_xrootd origin.cfg origin
    printf 'redirected file!\n' > target/file
}

teardown() {
    kill_pid_files
}

@test "stat discovers the new endpoint's capabilities after a redirect" {
    run -0 xrdfs http://127.0.0.1:8396 stat /file
    assert_output --partial 'Size:   17'

    # XrdHttp advertises PROPFIND, so stat uses it after discovering OPTIONS.
    run -0 grep -F 'got hdr line: OPTIONS / HTTP/1.1' origin.log
    run -0 grep -F 'got hdr line: PROPFIND /file HTTP/1.1' origin.log
    run -0 grep -F 'XrdHttpReq::Redir Redirecting to Location: http://127.0.0.1:8397/file' origin.log
    run -0 grep -F 'got hdr line: OPTIONS / HTTP/1.1' target.log
    run -0 grep -F 'got hdr line: PROPFIND /file HTTP/1.1' target.log
}
