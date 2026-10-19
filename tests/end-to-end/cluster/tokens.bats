#!/usr/bin/env bash

# A cluster which authorizes requests by the tokens of the test issuer: a
# meta manager, man1 with srv1 and srv2, man2 with srv3 and srv4. All nodes
# use TLS, ztn for root://, and serve HTTP. The tests share the cluster and
# one token, which grants all paths; each test has its own files.

load ../setup/cluster.bash

export X509_CERT_FILE="$CREDENTIALS/ca.pem"
export XrdSecPROTOCOL=ztn

setup_file() {
    require_plugin libXrdHttp
    require_plugin libXrdAccSciTokens
    require_commands curl
    start_issuer
    # xrdmapc logs in with ztn, thus the token comes before the cluster
    make_token 'storage.modify:/ storage.create:/ storage.read:/' 1800 > "$BATS_FILE_TMPDIR/token"
    chmod 0600 "$BATS_FILE_TMPDIR/token"
    export BEARER_TOKEN_FILE="$BATS_FILE_TMPDIR/token"
    local snippets=(http tls scitokens ztn)
    start_meta_manager metaman "${snippets[@]}"
    start_manager man1 metaman "${snippets[@]}"
    start_manager man2 metaman "${snippets[@]}"
    start_data_server srv1 man1 "${snippets[@]}"
    start_data_server srv2 man1 "${snippets[@]}"
    start_data_server srv3 man2 "${snippets[@]}"
    start_data_server srv4 man2 "${snippets[@]}"
    wait_for_cluster metaman 4
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    F="/t${BATS_TEST_NUMBER}.dat"
    TOKEN=$(<"$BATS_FILE_TMPDIR/token")
    make_random_file local.dat 100003
}

# Print the HTTP status of an upload of local.dat to the URL, with the token
# also after redirects
put() {
    curl --silent --location-trusted -o /dev/null -w '%{http_code}' \
        -H "Authorization: Bearer $TOKEN" -T local.dat "$1"
}

# Print the HTTP status of a MOVE of the URL to the URL with the suffix .new
move() {
    curl --silent --location-trusted -o /dev/null -w '%{http_code}' -X MOVE \
        -H "Authorization: Bearer $TOKEN" -H "Destination: $1.new" "$1"
}

@test "an HTTP PUT with a token through the meta manager creates the file" {
    run -0 put "$METAMAN_HTTP$F"
    assert_output 201
}

@test "an HTTP PUT with a token through a manager creates the file" {
    run -0 put "$MAN1_HTTP$F"
    assert_output 201
}

@test "an HTTP PUT with a token to a data server creates the file" {
    run -0 put "$SRV1_HTTP$F"
    assert_output 201
}

@test "an HTTP PUT without a token through the meta manager is forbidden" {
    run -0 curl --silent -L -o /dev/null -w '%{http_code}' -T local.dat "$METAMAN_HTTP$F"
    assert_output 403
}

@test "an HTTP MOVE with a token through the meta manager is not implemented" {
    cp local.dat "$SRV1_DATA$F"
    run -0 move "$METAMAN_HTTP$F"
    assert_output 501
}

@test "an HTTP MOVE with a token through a manager renames the file" {
    cp local.dat "$SRV1_DATA$F"
    run -0 move "$MAN1_HTTP$F"
    assert_output 201
}

@test "an HTTP MOVE with a token on a data server renames the file" {
    cp local.dat "$SRV1_DATA$F"
    run -0 move "$SRV1_HTTP$F"
    assert_output 201
}

@test "a roots upload with a token through the meta manager succeeds" {
    run -0 xrdcp local.dat "roots://localhost:$METAMAN_PORT/$F"
}

@test "a roots download with a token through the meta manager returns the file" {
    cp local.dat "$SRV3_DATA$F"
    xrdcp "roots://localhost:$METAMAN_PORT/$F" copy.dat
    run -0 cmp local.dat copy.dat
}

@test "a roots download without a token through the meta manager fails" {
    cp local.dat "$SRV3_DATA$F"
    BEARER_TOKEN_FILE="$BATS_TEST_TMPDIR/missing" run ! xrdcp "roots://localhost:$METAMAN_PORT/$F" copy.dat
}
