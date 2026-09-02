#!/usr/bin/env bash

X509_CERT_DIR="${BINARY_DIR}/tests/tls"
X509_USER_KEY="${X509_CERT_DIR}/client.key"
X509_USER_CERT="${X509_CERT_DIR}/client.crt"
XRD_HTTPCLIENTKEYFILE="${X509_USER_KEY}"
XRD_HTTPCLIENTCERTFILE="${X509_USER_CERT}"
MACAROONS_PLUGINCONFDIR="${PWD}/macaroons/client.plugins.d"
TOKEN_ISSUER_PORT=15044
TOKEN_ISSUER_HOST="https://localhost:${TOKEN_ISSUER_PORT}"
TOKEN_ISSUER_DAVS_HOST="davs://localhost:${TOKEN_ISSUER_PORT}"
TOKEN_ISSUER_TRACE="${PWD}/macaroons/token-issuer-trace.jsonl"
TOKEN_ISSUER_LOG="${PWD}/macaroons/token-issuer.log"

export XrdSecPROTOCOL X509_CERT_DIR X509_USER_KEY X509_USER_CERT
export XRD_HTTPCLIENTKEYFILE XRD_HTTPCLIENTCERTFILE

function setup_macaroons() {
	require_commands curl jq openssl python3

	mkdir -p "${MACAROONS_PLUGINCONFDIR}"
	cat >| "${MACAROONS_PLUGINCONFDIR}/http.conf" <<-EOF
	url = http://*;https://*;dav://*;davs://*
	lib = libXrdClHttp.so
	enable = true
	EOF

	cat >| macaroons.authdb <<-EOF
	u client /rw a
	u * / lr
	EOF

	cat >| macaroons.gridmap <<-EOF
	"/CN=client" client
	EOF

	cat >| "${REMOTE_DIR}/hello.txt" <<-EOF
	Hello, macaroons!
	EOF

	cat >| "${REMOTE_DIR}/deleteme.txt" <<-EOF
	Delete me if you can!
	EOF

	mkdir "${REMOTE_DIR}/rw"

	openssl rand -base64 -out macaroons.secret 64

	: > "${TOKEN_ISSUER_TRACE}"
	python3 "${SOURCE_DIR}/token_issuer_mock.py" \
		--bind 127.0.0.1 --port "${TOKEN_ISSUER_PORT}" \
		--cert "${X509_CERT_DIR}/host.pem" \
		--key "${X509_CERT_DIR}/host.key" \
		--trace-file "${TOKEN_ISSUER_TRACE}" \
		> "${TOKEN_ISSUER_LOG}" 2>&1 &
	echo "$!" > "${PWD}/macaroons/token-issuer.pid"

	local attempt
	for ((attempt = 0; attempt < 50; ++attempt)); do
		if curl -sf --capath "${X509_CERT_DIR}" \
			"${TOKEN_ISSUER_HOST}/healthz" >/dev/null; then
			return
		fi
		if ! kill -0 "$(cat "${PWD}/macaroons/token-issuer.pid")" 2>/dev/null; then
			break
		fi
		sleep 0.1
	done

	cat "${TOKEN_ISSUER_LOG}" >&2
	error "token issuer mock did not become ready"
}

function teardown_macaroons() {
	rm macaroons.{authdb,gridmap,secret}
	rm -f "${MACAROONS_PLUGINCONFDIR}/http.conf"
	rmdir "${MACAROONS_PLUGINCONFDIR}"
}

function reset_token_issuer_trace() {
	: > "${TOKEN_ISSUER_TRACE}"
}

function assert_token_issuer_trace() {
	local expected="$1"
	local actual
	actual=$(jq -cs \
		'map({method, url, content_type, accept, body})' \
		"${TOKEN_ISSUER_TRACE}") || \
		error "failed to parse token issuer trace"

	if [[ "${actual}" != "${expected}" ]]; then
		echo "expected token issuer trace: ${expected}" >&2
		echo "actual token issuer trace:   ${actual}" >&2
		error "token issuer workflow did not make the expected requests"
	fi
}

function test_token_issuer_workflows() {
	local response expected

	# A successful SciTokens endpoint completes after one discovery request and
	# one scope-free client-credentials POST.
	reset_token_issuer_trace
	response=$(xrdfs token --issuer "${TOKEN_ISSUER_HOST}/sci-success" \
		"${TOKEN_ISSUER_HOST}/storage/sci-object")
	assert_eq "sci-token" "${response}" \
		"SciTokens issuer returned an unexpected token"
	expected='['
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/sci-success","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"POST","url":"https://localhost:15044/token/sci-success","content_type":"application/x-www-form-urlencoded","accept":"application/json","body":"grant_type=client_credentials"}'
	expected+=']'
	assert_token_issuer_trace "${expected}"

	# If the SciTokens POST fails, gfal2 repeats RFC 8414 discovery before
	# making the scoped OAuth macaroon request. Use DAVS for the issuer to also
	# verify that it is normalized to HTTPS.
	reset_token_issuer_trace
	response=$(xrdfs token --issuer \
		"${TOKEN_ISSUER_DAVS_HOST}/oauth-success" --validity 2 \
		"${TOKEN_ISSUER_HOST}/storage/oauth-object?opaque=ignored")
	assert_eq "oauth-token" "${response}" \
		"OAuth macaroon issuer returned an unexpected token"
	expected='['
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/oauth-success","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"POST","url":"https://localhost:15044/token/oauth-success","content_type":"application/x-www-form-urlencoded","accept":"application/json","body":"grant_type=client_credentials"},'
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/oauth-success","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"POST","url":"https://localhost:15044/token/oauth-success","content_type":"application/x-www-form-urlencoded","accept":"application/json","body":"grant_type=client_credentials&expire_in=120&scopes=LIST%3A%2Fstorage%2Foauth-object%20DOWNLOAD%3A%2Fstorage%2Foauth-object"}'
	expected+=']'
	assert_token_issuer_trace "${expected}"

	# Both RFC 8414 attempts fail here. The OpenID discovery URL succeeds and
	# its endpoint receives the OAuth macaroon request.
	reset_token_issuer_trace
	response=$(xrdfs token --issuer \
		"${TOKEN_ISSUER_HOST}/openid-success" \
		"${TOKEN_ISSUER_HOST}/storage/openid-object")
	assert_eq "openid-token" "${response}" \
		"OpenID fallback returned an unexpected token"
	expected='['
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/openid-success","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/openid-success","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"GET","url":"https://localhost:15044/openid-success/.well-known/openid-configuration","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"POST","url":"https://localhost:15044/token/openid-success","content_type":"application/x-www-form-urlencoded","accept":"application/json","body":"grant_type=client_credentials&expire_in=3600&scopes=LIST%3A%2Fstorage%2Fopenid-object%20DOWNLOAD%3A%2Fstorage%2Fopenid-object"}'
	expected+=']'
	assert_token_issuer_trace "${expected}"

	# Direct storage issuance is attempted only after both RFC 8414 requests
	# and OpenID discovery fail.
	reset_token_issuer_trace
	response=$(xrdfs token --issuer \
		"${TOKEN_ISSUER_HOST}/direct-fallback" \
		"${TOKEN_ISSUER_HOST}/storage/direct-object?opaque=ignored")
	assert_eq "direct-token" "${response}" \
		"direct issuer fallback returned an unexpected token"
	expected='['
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/direct-fallback","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/direct-fallback","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"GET","url":"https://localhost:15044/direct-fallback/.well-known/openid-configuration","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"POST","url":"https://localhost:15044/storage/direct-object?opaque=ignored","content_type":"application/macaroon-request","accept":"*/*","body":"{\"caveats\": [\"activity:LIST,DOWNLOAD\"], \"validity\": \"PT60M\"}"}'
	expected+=']'
	assert_token_issuer_trace "${expected}"

	# Once discovery returns a usable OAuth endpoint, failure of its scoped POST
	# is terminal. In particular, the target storage URL must not be contacted.
	reset_token_issuer_trace
	assert_failure xrdfs token --issuer \
		"${TOKEN_ISSUER_HOST}/oauth-failure" \
		"${TOKEN_ISSUER_HOST}/storage/must-not-be-called"
	expected='['
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/oauth-failure","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"POST","url":"https://localhost:15044/token/oauth-failure","content_type":"application/x-www-form-urlencoded","accept":"application/json","body":"grant_type=client_credentials"},'
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/oauth-failure","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"POST","url":"https://localhost:15044/token/oauth-failure","content_type":"application/x-www-form-urlencoded","accept":"application/json","body":"grant_type=client_credentials&expire_in=3600&scopes=LIST%3A%2Fstorage%2Fmust-not-be-called%20DOWNLOAD%3A%2Fstorage%2Fmust-not-be-called"}'
	expected+=']'
	assert_token_issuer_trace "${expected}"

	# The request timeout is one deadline for the entire issuer workflow, not a
	# fresh timeout for each fallback stage. Each discovery response arrives
	# within one second, but the two together exceed the operation deadline.
	reset_token_issuer_trace
	assert_failure env XRD_REQUESTTIMEOUT=1 xrdfs token --issuer \
		"${TOKEN_ISSUER_HOST}/slow-deadline" \
		"${TOKEN_ISSUER_HOST}/storage/deadline-object"
	expected='['
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/slow-deadline","content_type":"","accept":"*/*","body":""},'
	expected+='{"method":"GET","url":"https://localhost:15044/.well-known/oauth-authorization-server/slow-deadline","content_type":"","accept":"*/*","body":""}'
	expected+=']'
	assert_token_issuer_trace "${expected}"
}

# Obtain a macaroon for $1 (path, default "/") with an optional JSON caveats
# array element $2.  Sets MACAROON on success.
function get_macaroon() {
	local path="${1:-/}"
	local caveats="${2:-}"
	local body

	if [[ -n "$caveats" ]]; then
		body='{"validity":"PT1H","caveats":['"$caveats"']}'
	else
		body='{"validity":"PT1H"}'
	fi

	curl -vf \
		--capath "${X509_CERT_DIR}" --cert "${X509_USER_CERT}" --key "${X509_USER_KEY}" \
		-H 'Content-Type: application/macaroon-request' \
		-X POST -d "$body" "${HOST}${path}" -o macaroon.response

	MACAROON=$(macaroon.response | jq -r '.macaroon' < macaroon.response)

	cat macaroon.response
	echo -e "\nmacaroon: \"${MACAROON}\""
	rm macaroon.response

	if [[ -n "$MACAROON" && "$MACAROON" != "null" ]]; then
		return
	fi

	error "Failed to obtain macaroon"
}

function test_macaroons() {
	HOST="https://localhost:15043"
	export XRD_PLUGINCONFDIR="${MACAROONS_PLUGINCONFDIR}"

	local response xrdfs_macaroon

	# Issuance tests

	# xrdfs uses the same local HTTPS macaroon endpoint as gfal-token.
	xrdfs_macaroon=$(xrdfs token "${HOST}/")
	[[ -n "${xrdfs_macaroon}" ]] || error "xrdfs returned an empty macaroon"

	# Tokens and request bodies must only be returned to the caller, not copied
	# into the XrdCl client log.
	if grep -Fq "${xrdfs_macaroon}" "${XRD_LOGFILE}"; then
		error "xrdfs logged the issued macaroon"
	fi
	if grep -Fq '"caveats"' "${XRD_LOGFILE}"; then
		error "xrdfs logged the macaroon request body"
	fi

	# The write defaults and explicit activity list are both accepted by the
	# local issuer; these requests do not perform a remote write.
	response=$(xrdfs token --write --validity 15 "${HOST}/rw")
	[[ -n "${response}" ]] || error "xrdfs returned an empty write macaroon"
	response=$(xrdfs token --validity 5 "${HOST}/" DOWNLOAD LIST)
	[[ -n "${response}" ]] || error "xrdfs returned an empty custom macaroon"
	response=$(xrdfs "${HOST}" token --validity 1 /)
	[[ -n "${response}" ]] || error "legacy xrdfs syntax returned an empty macaroon"

	test_token_issuer_workflows

	# A same-host issuer first receives the SciTokens client-credentials
	# request, which this macaroon endpoint rejects because it has no scope.
	# xrdfs must then repeat discovery and complete the OAuth macaroon flow.
	response=$(xrdfs token --issuer "${HOST}" --validity 5 "${HOST}/")
	[[ -n "${response}" ]] || error "issuer workflow returned an empty token"
	if grep -Fq "${response}" "${XRD_LOGFILE}"; then
		error "xrdfs logged the issuer-provided token"
	fi

	# Discovery below an unknown issuer path fails at both RFC 8414 and OIDC
	# endpoints, so the workflow falls back to direct storage issuance.
	response=$(xrdfs token --issuer "${HOST}/missing-issuer" "${HOST}/")
	[[ -n "${response}" ]] || error "issuer fallback returned an empty token"
	if grep -Fq "${response}" "${XRD_LOGFILE}"; then
		error "xrdfs logged the direct-fallback token"
	fi

	# Keep the OAuth endpoint compatible with both the standard singular
	# spelling and the plural spelling emitted by gfal-token.
	response=$(curl -sf \
		--capath "${X509_CERT_DIR}" --cert "${X509_USER_CERT}" --key "${X509_USER_KEY}" \
		-H 'Content-Type: application/x-www-form-urlencoded' \
		-X POST -d 'grant_type=client_credentials&expire_in=300&scope=DOWNLOAD%3A%2F' \
		"${HOST}/.oauth2/token")
	response=$(echo "${response}" | jq -er \
		'.access_token | select(type == "string" and length > 0)')
	[[ -n "${response}" ]] || error "singular OAuth scope returned an empty macaroon"

	response=$(curl -sf \
		--capath "${X509_CERT_DIR}" --cert "${X509_USER_CERT}" --key "${X509_USER_KEY}" \
		-H 'Content-Type: application/x-www-form-urlencoded' \
		-X POST -d 'grant_type=client_credentials&expire_in=300&scopes=LIST%3A%2F%20DOWNLOAD%3A%2F' \
		"${HOST}/.oauth2/token")
	response=$(echo "${response}" | jq -er \
		'.access_token | select(type == "string" and length > 0)')
	[[ -n "${response}" ]] || error "plural OAuth scopes returned an empty macaroon"

	# Exercise direct curl handle reuse in one process: the GET following the
	# token POST must remain a GET after the easy handle returns to the pool.
	response=$(printf 'token /\ncat /hello.txt\nexit\n' | xrdfs "${HOST}")
	[[ "${response}" == *"Hello, macaroons!"* ]] || \
		error "a read following direct token issuance did not return the file"

	# Exercise all issuer stages and handle reuse in one process as well: the
	# file GET following discovery and token POSTs must remain a GET after the
	# easy handles return to the pool.
	response=$(printf 'token --issuer %s /\ncat /hello.txt\nexit\n' "${HOST}" | \
		xrdfs "${HOST}")
	[[ "${response}" == *"Hello, macaroons!"* ]] || \
		error "a read following issuer token issuance did not return the file"

	# Reject insecure storage and issuer endpoints before making a token request.
	assert_failure xrdfs token "http://localhost:15043/"
	assert_failure xrdfs token --issuer http://localhost:15043 "${HOST}/"
	assert_failure xrdfs token --issuer \
		"https://client@localhost:15043" "${HOST}/"
	assert_failure xrdfs token --issuer= "${HOST}/"
	assert_failure xrdfs token --validity -1 "${HOST}/"
	assert_failure xrdfs token --val 5 "${HOST}/"

	# can obtain a macaroon via HTTPS POST
	get_macaroon /
	[[ -n "$MACAROON" ]] || error "Failed to obtain macaroon for /"

	# macaroon response includes expires_in field
	response=$(curl -sf --capath "${X509_CERT_DIR}" -X POST -d '{ "validity":"PT30S" }' \
		-H 'Content-Type: application/macaroon-request' "${HOST}/")
	echo "${response}" | jq -e '.expires_in == 30'

	# macaroon request without validity is rejected with HTTP error
	assert_failure curl -S -if --capath "${X509_CERT_DIR}" -X POST -d '{}' \
		-H 'Content-Type: application/macaroon-request' "${HOST}/"

	# macaroon request with reserved caveat 'path:' is rejected
	assert_failure curl -S -if --capath "${X509_CERT_DIR}" -X POST -d '{"validity":"PT1H","caveats":["path:/secret"]}' \
		-H 'Content-Type: application/macaroon-request' "${HOST}/"

	# macaroon request with reserved caveat 'name:' is rejected
	assert_failure curl -S -if --capath "${X509_CERT_DIR}" -X POST -d '{"validity":"PT1H","caveats":["name:root"]}' \
		-H 'Content-Type: application/macaroon-request' "${HOST}/"

	# macaroon request with unsupported caveat type is rejected
	assert_failure curl -S -if --capath "${X509_CERT_DIR}" -X POST -d '{"validity":"PT1H","caveats":["foobar:baz"]}' \
		-H 'Content-Type: application/macaroon-request' "${HOST}/"

	# Access tests

	# can read a file using a DOWNLOAD macaroon
	unset MACAROON
	get_macaroon / '"activity:DOWNLOAD,LIST"'
	assert curl -S -if --capath "${X509_CERT_DIR}" -H "Authorization: Bearer ${MACAROON}" "${HOST}/hello.txt"

	# reading without a macaroon is permitted (authdb allows anonymous reads)
	assert curl -S -if --capath "${X509_CERT_DIR}" "${HOST}/hello.txt"

	# macaroon restricted to a path cannot access a file outside that path
	unset MACAROON
	get_macaroon /subdir/
	assert_failure curl -S -if --capath "${X509_CERT_DIR}" -H "Authorization: Bearer ${MACAROON}" "${HOST}/hello.txt"

	# uploading without a macaroon is denied for anonymous users
	echo "upload attempt" >| upload.txt
	assert_failure curl -S -if --capath "${X509_CERT_DIR}" -X PUT -T upload.txt "${HOST}/upload.txt"

	# macaroon with UPLOAD activity cannot write beyond authdb permissions
	unset MACAROON
	get_macaroon / '"activity:MANAGE,UPLOAD"'
	echo "exploit content" >| exploit.txt
	assert_failure curl -S -if --capath "${X509_CERT_DIR}" -H "Authorization: Bearer ${MACAROON}" \
		-X PUT -T exploit.txt "${HOST}/exploit.txt"

	# macaroon with UPLOAD activity can write where authdb permits
	unset MACAROON
	get_macaroon /rw '"activity:MANAGE,UPLOAD"'
	echo "exploit content" >| exploit.txt
	assert curl -S -if --capath "${X509_CERT_DIR}" -H "Authorization: Bearer ${MACAROON}" \
		-X PUT -T exploit.txt "${HOST%/}/rw/exploit.txt"

	# macaroon with DELETE activity cannot delete beyond authdb permissions
	unset MACAROON
	assert get_macaroon / '"activity:DELETE"'
	assert_failure curl -S -if --capath "${X509_CERT_DIR}" -H "Authorization: Bearer ${MACAROON}" \
		-X DELETE "${HOST}/deleteme.txt"

	# macaroon with MANAGE activity cannot create directories beyond authdb permissions
	unset MACAROON
	assert get_macaroon / '"activity:MANAGE"'
	assert_failure curl -S -if --capath "${X509_CERT_DIR}" -H "Authorization: Bearer ${MACAROON}" \
		-X MKCOL "${HOST}/newdir/"

	# macaroon with MANAGE activity can create directories if authdb permits
	unset MACAROON
	assert get_macaroon /rw '"activity:MANAGE"'
	assert curl -S -if --capath "${X509_CERT_DIR}" -H "Authorization: Bearer ${MACAROON}" \
		-X MKCOL "${HOST%/}/rw/newdir/"

	# all tests passed, exit successfully
	exit 0
}
