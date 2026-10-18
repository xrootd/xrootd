#!/usr/bin/env bash

# Authorization by tokens over roots:// (with ztn) and over HTTPS. The tokens
# of the issuer of the setup grant paths below /protected. The tests share the
# issuer and one server.

load ../setup/server.bash

export X509_CERT_FILE="$CREDENTIALS/ca.pem"
export XrdSecPROTOCOL=ztn

setup_file() {
    require_plugin libXrdAccSciTokens
    require_commands curl

    start_issuer

    local hash
    hash=$(openssl x509 -in "$CREDENTIALS/host.pem" -noout -subject_hash)

    cat > "$BATS_FILE_TMPDIR/protected.conf" <<CONF
[Global]
audience = https://xrootd.test

[Issuer test]
issuer = $ISSUER
base_path = /protected
CONF

    # The holder of the host certificate, as the client of a TLS connection,
    # has the hash of the subject as its name.
    printf 'u %s.0 /localhost lr /protected lr\n' "$hash" > "$BATS_FILE_TMPDIR/protected.authdb"

    start_server server server http tls scitokens.cfg

    mkdir -p "$SERVER_DATA/protected" "$SERVER_DATA/localhost"
    echo 'Hello, World' > "$SERVER_DATA/protected/hello_world.txt"
    echo 'Hello, World' > "$SERVER_DATA/localhost/hello_world.txt"

    make_token storage.read:/ > "$BATS_FILE_TMPDIR/read.token"
    make_token storage.create:/subdir > "$BATS_FILE_TMPDIR/create.token"
    make_token storage.modify:/subdir_modify > "$BATS_FILE_TMPDIR/modify.token"
    chmod 0600 "$BATS_FILE_TMPDIR"/*.token
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    N=$BATS_TEST_NUMBER
    ROOTS="roots://localhost:$SERVER_PORT"
    echo 'token test' > local.txt
}

# Print the HTTP status of a request with the token file $1 (or none, for "")
# and the curl arguments after it
status() {
    local token=$1
    shift
    curl --silent --cacert "$CREDENTIALS/ca.pem" -o "$BATS_TEST_TMPDIR/body" -w '%{http_code}' \
        ${token:+-H "Authorization: Bearer $(<"$BATS_FILE_TMPDIR/$token.token")"} "$@"
}

#
# root:// with ztn
#

@test "a roots upload without a token fails" {
    run ! xrdcp local.txt "$ROOTS//protected/subdir/root$N.txt"
}

@test "a roots upload with a create token succeeds" {
    BEARER_TOKEN_FILE="$BATS_FILE_TMPDIR/create.token" run -0 xrdcp local.txt "$ROOTS//protected/subdir/root$N.txt"
}

@test "a create token cannot read over roots" {
    BEARER_TOKEN_FILE="$BATS_FILE_TMPDIR/create.token" xrdcp local.txt "$ROOTS//protected/subdir/root$N.txt"
    BEARER_TOKEN_FILE="$BATS_FILE_TMPDIR/create.token" run ! xrdcp "$ROOTS//protected/subdir/root$N.txt" copy.txt
}

@test "a read token reads the uploaded file over roots" {
    BEARER_TOKEN_FILE="$BATS_FILE_TMPDIR/create.token" xrdcp local.txt "$ROOTS//protected/subdir/root$N.txt"
    BEARER_TOKEN_FILE="$BATS_FILE_TMPDIR/read.token" xrdcp "$ROOTS//protected/subdir/root$N.txt" copy.txt
    run -0 cmp local.txt copy.txt
}

@test "a missing BEARER_TOKEN_FILE makes the client use the token in XDG_RUNTIME_DIR" {
    mkdir -m 0700 runtime
    cp "$BATS_FILE_TMPDIR/create.token" "runtime/bt_u$(id -u)"
    BEARER_TOKEN_FILE="$BATS_TEST_TMPDIR/missing.token" XDG_RUNTIME_DIR="$BATS_TEST_TMPDIR/runtime" \
        run -0 xrdcp local.txt "$ROOTS//protected/subdir/root$N.txt"
}

#
# HTTPS
#

@test "a client certificate reads a file which the authorization database allows" {
    run -0 status '' --cert "$CREDENTIALS/host.pem" --key "$CREDENTIALS/host.key" "$SERVER_HTTPS/localhost/hello_world.txt"
    assert_output 200
}

@test "the issuer publishes its keys" {
    run -0 status '' "$ISSUER/issuer.jwks"
    assert_output 200
}

@test "a read without a token is forbidden" {
    run -0 status '' "$SERVER_HTTPS/protected/hello_world.txt"
    assert_output 403
}

@test "a read with a read token succeeds" {
    run -0 status read "$SERVER_HTTPS/protected/hello_world.txt"
    assert_output 200
}

@test "a read with a read token returns the file" {
    status read "$SERVER_HTTPS/protected/hello_world.txt"
    assert_equal "$(<body)" 'Hello, World'
}

@test "a read with a create token is forbidden" {
    run -0 status create "$SERVER_HTTPS/protected/hello_world.txt"
    assert_output 403
}

@test "an upload with a create token creates the file" {
    run -0 status create -X PUT -d 'hello, world' "$SERVER_HTTPS/protected/subdir/t$N.txt"
    assert_output 201
}

@test "a read token reads a file which a create token uploaded" {
    status create -X PUT -d 'hello, world' "$SERVER_HTTPS/protected/subdir/t$N.txt"
    status read "$SERVER_HTTPS/protected/subdir/t$N.txt"
    assert_equal "$(<body)" 'hello, world'
}

@test "a second upload with a create token is forbidden" {
    status create -X PUT -d 'hello, world' "$SERVER_HTTPS/protected/subdir/t$N.txt"
    run -0 status create -X PUT -d 'hello, world' "$SERVER_HTTPS/protected/subdir/t$N.txt"
    assert_output 403
}

@test "an upload with a modify token creates the file" {
    run -0 status modify -X PUT -d test "$SERVER_HTTPS/protected/subdir_modify/t$N.txt"
    assert_output 201
}

@test "a second upload with a modify token replaces the file" {
    status modify -X PUT -d test "$SERVER_HTTPS/protected/subdir_modify/t$N.txt"
    run -0 status modify -X PUT -d 'hello, world' "$SERVER_HTTPS/protected/subdir_modify/t$N.txt"
    assert_output 201
}

@test "a read token reads the file which a modify token replaced" {
    status modify -X PUT -d test "$SERVER_HTTPS/protected/subdir_modify/t$N.txt"
    status modify -X PUT -d 'hello, world' "$SERVER_HTTPS/protected/subdir_modify/t$N.txt"
    status read "$SERVER_HTTPS/protected/subdir_modify/t$N.txt"
    assert_equal "$(<body)" 'hello, world'
}
