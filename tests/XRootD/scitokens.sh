#!/usr/bin/env bash

function setup_scitokens() {
	require_commands curl

	# Setup the issuer and protected directory
	ln -sf "$PWD/../issuer/export" scitokens/xrootd/issuer
	mkdir -p scitokens/xrootd/protected
	echo 'Hello, World' > scitokens/xrootd/protected/hello_world.txt

	# Override the scitoken cache location; otherwise, contents of prior test runs may be cached
	XDG_CACHE_HOME="${NAME}/cache"
	export XDG_CACHE_HOME
	mkdir -p "$XDG_CACHE_HOME/scitokens"

	# Add the xrdscitokens-create-token command to the PATH to simplify invocations below.
	PATH="$PWD/../scitokens:$PATH"

	# Create a read-only token
	OUTPUTDIR=$PWD/scitokens
	pushd ../issuer || exit 1
	if ! xrdscitokens-create-token issuer_pub_1.pem issuer_key_1.pem test_1 \
		"https://localhost:7095/issuer/one" storage.read:/ > "$OUTPUTDIR/token"; then
		echo "Failed to create token"
		exit 1
	fi
	chmod 0600 "$OUTPUTDIR/token"

	popd || exit 1
}

function execute_curl() {
	url="$1"
	expected_status="$2"
	expected_contents="$3"
	token_file="$4"

	if [ -n "$token_file" ]; then
		token_arg=(-H "Authorization: Bearer $(cat "$token_file")")
	fi

	# This does not use the "assert" helper function as we must redirect curl's output;
	# "assert" does not allow redirects
	echo "Downloading $url (expected $expected_status)"
	if ! curl -s "${token_arg[@]}" -o "$TMPDIR/curl_output" -w "%{http_code}" \
		--cacert "../issuer/tlsca.pem" "$url" > "$TMPDIR/curl_status"; then

		echo "Download failed.  Retrying with verbose output enabled:"
		curl -s "${token_arg[@]}" -v --cacert "../issuer/tlsca.pem" "$url"
		exit 1
	fi
	assert_eq "$expected_status" "$(cat "$TMPDIR/curl_status")"
	if [ -n "$expected_contents" ]; then
		assert_eq "$expected_contents" "$(cat "$TMPDIR/curl_output")"
	fi
}

function test_scitokens() {
	# The default $HOST construction uses $HOSTNAME in the URL; that will
	# fail here because of the TLS verification (the certificate is generated
	# with "localhost" instead).
	HOST="roots://localhost:${XRD_PORT}/"
	export X509_CERT_FILE=../issuer/tlsca.pem
	export BEARER_TOKEN_FILE=scitokens/token
	echo
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	# create local temporary directory
	TMPDIR=$(mktemp -d "${NAME}/test-XXXXXX")

	# from now on, we use HTTP
	export HOST=https://localhost:7095

	execute_curl "$HOST/issuer/one/issuer.jwks" 200 "$(cat scitokens/xrootd/issuer/one/issuer.jwks)"
	execute_curl "$HOST/protected/hello_world.txt" 403 ""
	execute_curl "$HOST/protected/hello_world.txt" 200 "Hello, World" scitokens/token
}
