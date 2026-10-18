#!/usr/bin/env bash

# credentials.sh create|remove <dir>
#
# Create the credentials which all tests share, or remove them. The tests only
# read them, thus one set serves a whole run of the test suite. The files are
# read-only after creation:
#
#   ca.pem, ca.key           certificate authority (CA), and the hash links of
#                            ca.pem and root.crl for a CA directory (-CApath)
#   host.pem, host.key       server certificate, CN=localhost, with the names
#                            localhost, the host name, 127.0.0.1 and ::1
#   client.crt, client.key   client certificate, CN=client
#   invalid.crt, .key        client certificate without the client extensions
#   revoked.crt, .key        client certificate which root.crl revokes
#   root.crl                 certificate revocation list (CRL) of the CA
#   gridmap                  maps "/CN=client" to the user client
#   issuer.key, issuer.pub   EC key of a token issuer, with the key ID "test"
#   issuer.jwks              public key of the issuer as a JSON Web Key Set,
#                            if the build has xrdscitokens-create-jwks
#   macaroon.secret          secret key for macaroons
#
# The host names come from the host at the time of creation, as the clients of
# some tests use them.

set -e

command=$1
dir=$2

[[ -n "$dir" ]] || { echo "usage: $0 create|remove <dir>" >&2; exit 2; }

remove() {
    [[ -e "$dir" ]] || return 0
    chmod -R u+w "$dir"
    rm -rf "$dir"
}

certificate() {
    local name=$1 cn=$2 ext=$3

    openssl genrsa -out "$name.key" 2048 2>/dev/null
    openssl req -config "$conf" -new -key "$name.key" -subj "/CN=$cn" -out "$name.csr"
    openssl ca -batch -config "$conf" ${ext:+-extensions "$ext"} -in "$name.csr" \
        -out "$name.crt" 2>/dev/null
    rm -f "$name.csr"
}

create() {
    local conf
    conf="$(cd "$(dirname "$0")" && pwd)/credentials.conf"

    remove
    mkdir -p "$dir/ca"
    cd "$dir"
    touch ca/db

    export CREDENTIALS_DIR=$PWD
    export CREDENTIALS_HOST
    export CREDENTIALS_FQDN
    # Some containers have no hostname command (Fedora), but all have uname.
    CREDENTIALS_FQDN=$(hostname -f 2>/dev/null || uname -n)
    CREDENTIALS_HOST=${CREDENTIALS_FQDN%%.*}

    openssl genrsa -out ca.key 2048 2>/dev/null
    openssl req -x509 -config "$conf" -key ca.key -extensions ca_ext -days 30 -out ca.pem

    certificate host localhost host_ext
    mv host.crt host.pem
    certificate client client client_ext
    certificate invalid invalid
    certificate revoked revoked client_ext

    openssl ca -batch -config "$conf" -revoke revoked.crt 2>/dev/null
    openssl ca -batch -config "$conf" -gencrl -out root.crl 2>/dev/null
    openssl rehash . >/dev/null

    openssl verify -CAfile ca.pem -x509_strict host.pem client.crt >/dev/null
    # set -e ignores a command negated with !, thus test the status
    if openssl verify -CApath . -crl_check revoked.crt >/dev/null 2>&1; then
        echo "$0: the CRL does not revoke revoked.crt" >&2
        exit 1
    fi

    echo '"/CN=client" client' > gridmap

    openssl ecparam -name prime256v1 -genkey -noout -out issuer.key
    openssl ec -in issuer.key -pubout -out issuer.pub 2>/dev/null
    if command -v xrdscitokens-create-jwks >/dev/null; then
        xrdscitokens-create-jwks issuer.pub issuer.jwks test >/dev/null
    fi

    openssl rand -base64 -out macaroon.secret 64

    rm -rf ca
    # XRootD requires keys which only the owner can read, and a CA directory
    # which others cannot write
    chmod 0400 ./*.key
    find . -maxdepth 1 -type f ! -name '*.key' -exec chmod 0444 {} +
    chmod 0550 .
}

case "$command" in
    create) create ;;
    remove) remove ;;
    *) echo "usage: $0 create|remove <dir>" >&2; exit 2 ;;
esac
