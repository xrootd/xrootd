#!/usr/bin/env bash

function setup_scitokens() {
	require_commands curl
	if [ -z "$HOSTNAME" ]; then
		HOSTNAME=`hostname`
	fi

	cat > "$PWD/scitokens/authdb" << EOF
u * /issuer lr
EOF
	cat > "$PWD/scitokens/scitokens.cfg" <<EOF
[Global]
audience = https://$HOSTNAME:8095

[Issuer test]
issuer = https://$HOSTNAME:8095/issuer/one
base_path = /protected
EOF

	# Setup the issuer and protected directory
	ln -sf "$PWD/../issuer/export" "$PWD/scitokens/xrootd/issuer"
	mkdir -p "$PWD/scitokens/xrootd/protected"
	echo 'Hello, World' > "$PWD/scitokens/xrootd/protected/hello_world.txt"

	# Override the scitoken cache location; otherwise, contents of prior test runs may be cached
	export XDG_CACHE_HOME=$(mktemp -d "${PWD}/${NAME}/cache-XXXXXX")
	mkdir -p "$XDG_CACHE_HOME/scitokens"

	$PWD/../scitokens/xrdscitokens-create-token ../issuer/issuer_pub_1.pem ../issuer/issuer_key_1.pem test_1 https://$HOSTNAME:8095/issuer/one storage.read:/ > "$PWD/scitokens/token"
	if [ "$?" -ne 0 ]; then
		echo "Failed to create token"
		exit 1
	fi
}

function teardown_scitokens() {
	echo
}

function test_scitokens() {
	echo
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	if [ -z "$HOSTNAME" ]; then
		HOSTNAME=`hostname`
	fi

	# create local temporary directory
	TMPDIR=$(mktemp -d "${PWD}/${NAME}/test-XXXXXX")

	# from now on, we use HTTP
	export HOST=https://$HOSTNAME:8095

	echo "Downloading $HOST/issuer/one/issuer.jwks"
	curl -s -o /dev/null -w "%{http_code}" --cacert "$PWD/../issuer/tlsca.pem" "$HOST/issuer/one/issuer.jwks" > $TMPDIR/jwks_status.txt
	if [ "$?" -ne 0 ]; then
		echo "Failed to download $HOST/issuer/one/issuer.jwks"
		exit 1
	fi
	assert_eq 200 `cat "$TMPDIR/jwks_status.txt"`

	echo "Downloading $HOST/protected/hello_world.txt (expected 403)"
	curl -s -o /dev/null -w "%{http_code}" --cacert "$PWD/../issuer/tlsca.pem" "$HOST/protected/hello_world.txt" > $TMPDIR/protected_status.txt
	assert_eq 403 `cat "$TMPDIR/protected_status.txt"`

	echo "Downloading $HOST/protected/hello_world.txt (expected 200)"
	curl -s -o "$TMPDIR/protected_contents.txt" -w "%{http_code}" --cacert "$PWD/../issuer/tlsca.pem" -H "Authorization: Bearer `cat "$PWD/scitokens/token"`" "$HOST/protected/hello_world.txt" > "$TMPDIR/protected_status.txt"
	assert_eq 200 `cat "$TMPDIR/protected_status.txt"`
	assert_eq "Hello, World" "`cat "$TMPDIR/protected_contents.txt"`"
}
