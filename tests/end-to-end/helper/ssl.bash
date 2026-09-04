#!/usr/bin/env bash

generate_ca_files() {
    local subject="/C=CH/ST=Geneva/L=Geneva/O=XRootD/OU=IT/CN=localhost"

    openssl genrsa -out ca.key 4096
    openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.pem -subj "$subject"
}

generate_host_files() {
    local path=${1:-.}
    local subject="/C=CH/ST=Geneva/L=Geneva/O=XRootD/OU=IT/CN=localhost"

    openssl genrsa -out host.key 2048
    openssl req -new -key host.key -out host.csr -subj "$subject"

    openssl x509 -req -in host.csr -CA $path/ca.pem -CAkey $path/ca.key -CAcreateserial -out host.pem -days 825 -sha256 -subj "$subject"
}

generate_client_files() {
    local path=${1:-.}
    local subject="/CN=dummy"

    openssl genrsa -out client.key 2048
    openssl req -new -key client.key -out client.csr -subj "$subject"

    openssl x509 -req -in client.csr -CA $path/ca.pem -CAkey $path/ca.key -CAcreateserial -out client.pem -days 825 -sha256 -subj "$subject"
}
