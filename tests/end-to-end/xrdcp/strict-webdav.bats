#!/usr/bin/env bash

# This server requires the query on every path and can decline a subtree.
load ../setup/server.bash

# shellcheck disable=SC2034 # BATS reads it
BATS_TEST_TIMEOUT=45
export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPDISABLEX509=1 X509_USER_PROXY=/dev/null/x509
export no_proxy=localhost,127.0.0.1 NO_PROXY=localhost,127.0.0.1

setup() {
    require_plugin libXrdClHttp
    require_commands python3
    cd "$BATS_TEST_TMPDIR"
    python3 "$BATS_TEST_DIRNAME/directory-server.py" "$BATS_TEST_TMPDIR/port" > requests.log 2>&1 3>&- 4>&- &
    server_pid=$!
    wait_for 5 test -s port
    endpoint="http://127.0.0.1:$(cat port)"
}

teardown() {
    if [[ -n ${server_pid:-} ]]; then
        stop_pid "$server_pid"
    fi
}

@test "an upload to a directory preserves its query during the probe and transfer" {
    require_commands curl
    printf 'upload data\n' > upload.bin
    xrdcp upload.bin "$endpoint/dst?authz=write"
    curl -fsS "$endpoint/dst/upload.bin?authz=write" -o received.bin
    run -0 cmp upload.bin received.bin
}

@test "an upload uses the -OD query to probe a directory and transfer its file" {
    require_commands curl
    printf 'upload data\n' > upload.bin
    xrdcp -ODauthz=write upload.bin "$endpoint/dst"
    curl -fsS "$endpoint/dst/upload.bin?authz=write" -o received.bin
    run -0 cmp upload.bin received.bin
}

@test "an upload fails when the directory rejects its probe" {
    printf 'upload data\n' > upload.bin
    run ! xrdcp upload.bin "$endpoint/dst"
}

@test "a recursive upload adds -OD to the destination query for nested files" {
    require_commands curl
    mkdir -p upload-tree/nested
    printf 'uploaded leaf\n' > upload-tree/nested/leaf.bin
    xrdcp --recursive -ODauthz=write upload-tree "$endpoint/dst?test=path&extra=1"
    curl -fsS "$endpoint/dst/upload-tree/nested/leaf.bin?authz=write" -o received.bin
    run -0 cmp upload-tree/nested/leaf.bin received.bin
}

check_download() {
    mkdir download
    xrdcp --recursive "$@" download
    run -0 cat download/tree/good.bin
    assert_output 'good data'
    run -0 cat download/tree/nested/leaf.bin
    assert_output 'nested data'
}

@test "a recursive download passes the source query to child requests" {
    check_download "$endpoint/src/tree?authz=read"
}

@test "a recursive download passes -OS to child requests" {
    check_download -OSauthz=read "$endpoint/src/tree"
}

@test "a recursive download combines -OS with an existing source query" {
    check_download -OSauthz=read "$endpoint/src/tree?test=path&extra=1"
}

@test "a recursive download with a partial listing starts no file transfers" {
    mkdir download
    run ! xrdcp --recursive -OSauthz=read "$endpoint/src/partial" download
    run -0 ls -A download
    assert_output ''
    run -1 grep -F 'GET /src/partial/good.bin' requests.log
}

@test "a directory entry with a conflicting href fails the recursive download" {
    mkdir download
    run ! xrdcp --recursive -OSauthz=read "$endpoint/src/traversal" download
    run -0 ls -A download
    assert_output ''
    run -1 grep -F '/src/traversal/safe.bin' requests.log
    run ! test -e escaped
}

@test "a directory entry naming the parent fails the recursive download" {
    mkdir download
    run ! xrdcp --recursive -OSauthz=read "$endpoint/src/traversal-direct" download
    run -0 ls -A download
    assert_output ''
}
