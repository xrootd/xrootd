#!/usr/bin/env bash

function generate_tls() {
	local conf="$(dirname ${BASH_SOURCE[0]})/tls.conf"

    pushd $(pwd) 1>/dev/null

	cd $BATS_SUITE_TMPDIR

	mkdir -p ca
	touch ca/db
	# Create private key and root certificate for the CA
	openssl genrsa -out ca.key 4096
	openssl req -x509 -config $conf -key ca.key -extensions xrootd_ca_ext -outform PEM -out ca.pem
	openssl verify -CAfile ca.pem ca.pem

	# Create private key and certificate for the XRootD server
	openssl genrsa -out host.key 4096
	openssl req -config $conf -new -key host.key -outform PEM -out host.csr -subj '/CN=localhost'
	openssl ca -batch -config $conf -in host.csr -extensions xrootd_crt_ext -out host.pem
	openssl verify -CAfile ca.pem host.pem

	# Create private key and certificate for the XRootD client
	openssl genrsa -out client.key 4096
	openssl req -config $conf -new -key client.key -out client.csr -subj '/CN=client'
	openssl ca -batch -config $conf -in client.csr -extensions xrootd_usr_ext -out client.crt
	openssl verify -CAfile ca.pem client.crt

	# Create a bad certificate which misses the required xrootd_usr_ext extensions
	openssl genrsa -out invalid.key 4096
	openssl req -config $conf -new -key invalid.key -out invalid.csr -subj '/CN=invalid'
	openssl ca -batch -config $conf -in invalid.csr -out invalid.crt

	# Create a revoked certificate and a certificate revocation list
	openssl genrsa -out revoked.key 4096
	openssl req -config $conf -new -key revoked.key -out revoked.csr -subj '/CN=revoked'
	openssl ca -batch -config $conf -in revoked.csr -extensions xrootd_usr_ext -out revoked.crt
	openssl ca -batch -config $conf -revoke revoked.crt

	openssl ca -batch -config $conf -gencrl -keyfile ca.key -cert ca.pem -out root.crl
	openssl crl -in root.crl -noout -text

	# Create symlinks based on certificate hashes (needed by XRootD TLS initialization)
	openssl rehash .

	# Ensure that revoked certificate fails certificate verification
	openssl verify -CApath . -crl_check -x509_strict revoked.crt && exit 1

	# XRootD client/server expect restricted permissions on CA directory
	chmod 750 .

    popd 1>/dev/null
}
