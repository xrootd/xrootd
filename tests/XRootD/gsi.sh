#!/usr/bin/env bash

XrdSecPROTOCOL=gsi
X509_CERT_DIR="${BINARY_DIR}/tests/tls"
X509_USER_KEY="${X509_CERT_DIR}/client.key"
X509_USER_CERT="${X509_CERT_DIR}/client.crt"

export XrdSecPROTOCOL X509_CERT_DIR X509_USER_KEY X509_USER_CERT

function setup_gsi() {
	cat >| authdb <<-EOF
	u client / a
	EOF

	cat >| gridmap <<-EOF
	"/CN=client" client
	EOF

	pushd "${X509_CERT_DIR}" || exit 1
	xrdgsiproxy init -certdir . -cert client.crt -key client.key -out "${OLDPWD}/proxy.crt"
	xrdgsiproxy init -certdir . -cert invalid.crt -key invalid.key -out "${OLDPWD}/iproxy.crt"
	xrdgsiproxy init -certdir . -cert revoked.crt -key revoked.key -out "${OLDPWD}/rproxy.crt"
	popd || exit 1
}

function teardown_gsi() {
	pwd && ls && rm authdb gridmap proxy.{crt,crtp}
}

function test_gsi() {
	export X509_USER_PROXY=proxy.crt

	xrdgsitest

	# XRootD client expects either cert+key or proxy, not both
	unset X509_USER_CERT X509_USER_KEY

	echo
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	# Make sure we did authenticate with gsi
	assert grep -c "Authenticated with gsi" "${XRD_LOGFILE}"

	# Copy a file to the server and back
	assert xrdcp -f "${SOURCE_DIR}"/gsi.cfg "${HOST}/gsi.cfg"
	assert xrdcp -f "${HOST}"/gsi.cfg .

	# Show the configuration
	assert xrdfs "${HOST}" cat /gsi.cfg
	assert xrdfs "${HOST}" rm  /gsi.cfg

	# Check against original file
	assert diff -u "${SOURCE_DIR}"/gsi.cfg gsi.cfg

	assert truncate -s 0 "${XRD_LOGFILE}"

	# Check that authentication fails with a bad (invalid) proxy certificate
	assert_failure env X509_USER_PROXY=iproxy.crt xrdfs "${HOST}" query config version

	# Check that authentication fails with a bad (insecure) proxy certificate
	chmod 644 "${X509_USER_PROXY}"
	assert_failure xrdfs "${HOST/root/roots}" ls /
	assert grep -c "TLS error" "${XRD_LOGFILE}"
}
