#!/usr/bin/env bash

# Live counterpart of tests/XRootD/scitokens.sh for sec.protocol jwt.
#
# A second xrootd serves the issuer. The plugin fetches the JWKS at startup,
# so that issuer has to be up before the data server is. Tokens are ES256 and
# are minted with xrdjwt-create-token (OpenSSL only).
#
# Authorization is acc.authdb on the subject. This test does not load
# XrdAccjwt, so it does not check storage.read / storage.create /
# storage.modify.

# Keep this in sync with the issuer URL written below. The data server port
# is jwt.cfg (7114).
ISSUER_PORT=7112
ISSUER_URL="https://localhost:${ISSUER_PORT}/issuer/one"
AUDIENCE="https://localhost:${ISSUER_PORT}"

function setup_jwt() {
	require_commands curl openssl xrdjwt-create-token

	mkdir -p jwt/ca jwt/issuer-root/issuer/one/.well-known jwt/issuer-admin
	mkdir -p "${REMOTE_DIR}/protected/subdir"
	echo 'Hello, World' > "${REMOTE_DIR}/protected/hello_world.txt"

	pushd jwt/ca >/dev/null || exit 1

	openssl ecparam -name prime256v1 -genkey -noout -out issuer_key.pem
	openssl ec -in issuer_key.pem -pubout -out issuer_pub.pem
	xrdjwt-create-token jwks issuer_pub.pem ../issuer-root/issuer/one/issuer.jwks test_1

	openssl genrsa -out tlscakey.pem 4096
	mkdir -p ca
	touch ca/index.txt
	echo '01' > ca/serial.txt

	cat > tlsca.ini <<EOF
[ ca ]
default_ca = CA_test

[ CA_test ]
default_days = 365
default_md = sha256
private_key = ${PWD}/tlscakey.pem
certificate = ${PWD}/tlsca.pem
new_certs_dir = ${PWD}/ca
database = ${PWD}/ca/index.txt
serial = ${PWD}/ca/serial.txt

[ req ]
default_bits = 4096
distinguished_name = ca_test_dn
x509_extensions = ca_extensions
string_mask = utf8only
prompt = no

[ ca_test_dn ]
commonName = Xrootd CA

[ ca_extensions ]
basicConstraints = critical,CA:true
keyUsage = keyCertSign,cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid

[ signing_policy ]
countryName            = optional
stateOrProvinceName    = optional
localityName           = optional
organizationName       = optional
organizationalUnitName = optional
commonName             = supplied
emailAddress           = optional

[ cert_extensions ]
basicConstraints = critical,CA:false
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = critical, serverAuth, clientAuth
subjectAltName = DNS:localhost
EOF

	openssl req -x509 -key tlscakey.pem -config tlsca.ini -out tlsca.pem -outform PEM \
		-subj "/CN=XRootD CA" 0<&-
	openssl genrsa -out tls.key 4096
	chmod 0400 tls.key
	openssl req -new -key tls.key -config tlsca.ini -out tls.csr -outform PEM \
		-subj "/CN=localhost" 0<&-
	openssl ca -config tlsca.ini -batch -policy signing_policy -extensions cert_extensions \
		-out tls.crt -infiles tls.csr 0<&-
	chmod 0600 tls.crt

	popd >/dev/null || exit 1

	cat > jwt/issuer-root/issuer/one/.well-known/openid-configuration <<EOF
{"issuer":"${ISSUER_URL}","jwks_uri":"${ISSUER_URL}/issuer.jwks"}
EOF

	cat > jwt/scitokens.cfg <<EOF
[Global]
audience = ${AUDIENCE}

[Issuer test]
issuer = ${ISSUER_URL}
base_path = /protected
map_subject = true
EOF

	cat > jwt/issuer.cfg <<EOF
set name = jwtissuer
set port = ${ISSUER_PORT}
set pwd = \$PWD

xrd.protocol XrdHttp:${ISSUER_PORT} libXrdHttp.so
xrd.tlsca certfile \$pwd/jwt/ca/tlsca.pem
xrd.tls \$pwd/jwt/ca/tls.crt \$pwd/jwt/ca/tls.key
http.tlsclientauth off

all.sitename jwtissuer
all.export /
oss.localroot \$pwd/jwt/issuer-root
all.adminpath \$pwd/jwt/issuer-admin
all.pidpath \$pwd/jwt
xrd.port \$port
EOF

	# libcurl in the plugin does not always read SSL_CERT_FILE by itself.
	# fetchURL() passes this path as CURLOPT_CAINFO.
	export SSL_CERT_FILE="${PWD}/jwt/ca/tlsca.pem"
	export CURL_CA_BUNDLE="${SSL_CERT_FILE}"
	export X509_CERT_FILE="${SSL_CERT_FILE}"

	if ! xrootd -b -l "${PWD}/jwt/issuer.log" -s "${PWD}/jwt/issuer.pid" \
		-c "${PWD}/jwt/issuer.cfg" -n jwtissuer; then
		echo "failed to start issuer xrootd" >&2
		tail -n 40 jwt/issuer.log >&2 || true
		exit 1
	fi

	local ok=0
	for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
		if curl -sf --cacert jwt/ca/tlsca.pem \
			"${ISSUER_URL}/.well-known/openid-configuration" >/dev/null; then
			ok=1
			break
		fi
		sleep 0.25
	done
	if [[ "$ok" != 1 ]]; then
		echo "issuer did not serve ${ISSUER_URL}/.well-known/openid-configuration" >&2
		tail -n 40 jwt/issuer.log >&2 || true
		exit 1
	fi
}

function mint_token() {
	local sub="$1"
	local aud="$2"
	local lifetime="$3"
	local out="$4"
	xrdjwt-create-token token jwt/ca/issuer_key.pem test_1 \
		"$ISSUER_URL" "$aud" "$sub" "$lifetime" > "$out"
	chmod 0600 "$out"
}

function execute_curl() {
	url="$1"
	expected_status="$2"
	expected_contents="$3"
	token_file="$4"
	http_verb="$5"

	verb_arg=()
	if [ -n "$http_verb" ]; then
		verb_arg=(-X "$http_verb" -d "$expected_contents")
		echo -n "$http_verb $url "
	else
		echo -n "GET $url "
	fi
	token_arg=()
	if [ -n "$token_file" ]; then
		token_arg=(-H "Authorization: Bearer $(cat "$token_file")")
		echo -n "with token file $token_file "
	else
		echo -n "with no token "
	fi
	echo "(expected $expected_status)"

	if ! curl -s "${verb_arg[@]}" "${token_arg[@]}" -o "$TMPDIR/curl_output" -w "%{http_code}" \
		--cacert "jwt/ca/tlsca.pem" "$url" > "$TMPDIR/curl_status"; then
		echo "HTTP request failed. Retrying with verbose output enabled:"
		curl -s "${verb_arg[@]}" "${token_arg[@]}" -v --cacert "jwt/ca/tlsca.pem" "$url"
		exit 1
	fi
	assert_eq "$expected_status" "$(cat "$TMPDIR/curl_status")"
	if [ -n "$expected_contents" ] && [ -z "$http_verb" ]; then
		assert_eq "$expected_contents" "$(cat "$TMPDIR/curl_output")"
	fi
}

function test_jwt() {
	require_commands curl xrdjwt-create-token

	HOST="roots://localhost:${XRD_PORT}/"
	export X509_CERT_FILE="${PWD}/jwt/ca/tlsca.pem"

	TMPDIR=$(mktemp -d "${NAME}/test-XXXXXX")

	mint_token alice "$AUDIENCE" 3600 jwt/token
	mint_token bob "$AUDIENCE" 3600 jwt/token_bob
	mint_token alice "https://wrong.example" 3600 jwt/token_badaud
	mint_token alice "$AUDIENCE" -180 jwt/token_expired

	# HOST already ends in '/'. Another slash makes the path absolute
	# (roots://host:port//protected). A single slash is a relative path,
	# which the server rejects.
	export BEARER_TOKEN_FILE=jwt/token
	echo
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	unset BEARER_TOKEN_FILE
	assert_failure xrdcp "${SOURCE_DIR}/jwt.cfg" "${HOST}/protected/from-root.txt"

	export BEARER_TOKEN_FILE=jwt/token
	assert xrdcp "${SOURCE_DIR}/jwt.cfg" "${HOST}/protected/from-root.txt"
	assert xrdcp -f "${HOST}/protected/hello_world.txt" "${TMPDIR}/hello_world.txt"
	assert diff -u "${REMOTE_DIR}/protected/hello_world.txt" "${TMPDIR}/hello_world.txt"

	export BEARER_TOKEN_FILE=jwt/token_bob
	assert_failure xrdcp -f "${HOST}/protected/hello_world.txt" "${TMPDIR}/bob.txt"

	export BEARER_TOKEN_FILE=jwt/token_badaud
	assert_failure xrdcp -f "${HOST}/protected/hello_world.txt" "${TMPDIR}/badaud.txt"

	export BEARER_TOKEN_FILE=jwt/token_expired
	assert_failure xrdcp -f "${HOST}/protected/hello_world.txt" "${TMPDIR}/expired.txt"

	export HOST="https://localhost:${XRD_PORT}"
	execute_curl "$HOST/protected/hello_world.txt" 403 ""
	execute_curl "$HOST/protected/hello_world.txt" 200 "Hello, World" jwt/token
	execute_curl "$HOST/protected/hello_world.txt" 403 "" jwt/token_bob
	execute_curl "$HOST/protected/hello_world.txt" 401 "" jwt/token_badaud
	execute_curl "$HOST/protected/hello_world.txt" 401 "" jwt/token_expired
	execute_curl "$HOST/protected/subdir/uploaded.txt" 201 "uploaded" jwt/token PUT
	execute_curl "$HOST/protected/subdir/uploaded.txt" 200 "uploaded" jwt/token
}
