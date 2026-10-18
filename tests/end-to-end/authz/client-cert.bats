#!/usr/bin/env bash

# Client certificates over HTTPS. The server "on" asks for a certificate and
# names its holder by the hash of the subject, which the authorization
# database allows to read /protected. The server "off" asks for none, thus it
# does not know the holder. The tests share the two servers.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl

    local hash
    hash=$(openssl x509 -in "$CREDENTIALS/host.pem" -noout -subject_hash)
    printf 'u %s.0 /protected lr\n' "$hash" > "$BATS_FILE_TMPDIR/client-cert.authdb"

    start_server on server http tls client-cert.cfg
    start_server off server http tls client-cert.cfg no-client-cert.cfg

    mkdir "$ON_DATA/protected" "$OFF_DATA/protected"
    echo 'Hello, World' > "$ON_DATA/protected/hello_world.txt"
    echo 'Hello, World' > "$OFF_DATA/protected/hello_world.txt"
}

teardown_file() {
    stop_servers
}

# Print the HTTP status of a GET with the host certificate as client
# certificate
status_with_certificate() {
    curl --silent --cacert "$CREDENTIALS/ca.pem" --cert "$CREDENTIALS/host.pem" \
        --key "$CREDENTIALS/host.key" -o /dev/null -w '%{http_code}' "$1"
}

@test "a server which asks for a client certificate allows its holder to read" {
    run -0 status_with_certificate "$ON_HTTPS/protected/hello_world.txt"
    assert_output 200
}

@test "a server with tlsclientauth off does not know the holder of a client certificate" {
    run -0 status_with_certificate "$OFF_HTTPS/protected/hello_world.txt"
    assert_output 403
}
