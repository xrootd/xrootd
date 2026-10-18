#!/usr/bin/env bash

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPCERTFILE="$CREDENTIALS/ca.pem"

# The server "auth" demands a token for a read and for a write, over http or
# https. The server "noauth" demands no token.
setup_file() {
	require_plugin libXrdHttpTPC
	require_plugin libXrdMacaroons
	openssl rand -base64 -out "$BATS_FILE_TMPDIR/auth.secret" 64
	start_server auth server http tls https-tpc macaroons authz.cfg secret.cfg
	start_server noauth server http tls https-tpc roots.cfg
	echo 'source content' > "$AUTH_DATA/file_src"
	echo 'source content' > "$NOAUTH_DATA/file_src"
}

teardown_file() {
	stop_servers
}

# Each test has its own destination file
setup() {
	cd "$BATS_TEST_TMPDIR"
	N=$BATS_TEST_NUMBER

	auth_token=$(request_macaroon "$AUTH_HTTPS/")

	printf '%s\n\n' "$auth_token" > token-file-src
	printf '\n%s\n' "$auth_token" > token-file-dst
	printf '%s\n%s\n' "$auth_token" "$auth_token" > token-file-both
	printf '\n\n' > token-file-empty

	jq -n --arg src "$auth_token" '{ src: $src }' > token-file-src.json
	jq -n --arg dst "$auth_token" '{ dst: $dst }' > token-file-dst.json
	echo '{ "token": "invalid" }' > token-file-invalid.json
}

@test "pull copy from auth to noauth without a token fails" {
	run ! xrdcp -T only $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "pull copy from noauth to auth without a token fails" {
	run ! xrdcp -T only $NOAUTH_HTTPS//file_src $AUTH_HTTPS//file_dst$N
}

@test "pull copy from auth to noauth with --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-src $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "pull copy from noauth to auth with --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src $NOAUTH_HTTPS//file_src $AUTH_HTTPS//file_dst$N
}

@test "pull copy from auth to noauth with --tpc-token-file without the source token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-dst $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "pull copy from noauth to auth with --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-dst $NOAUTH_HTTPS//file_src $AUTH_HTTPS//file_dst$N
}

@test "pull copy from auth to noauth with a JSON --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-src.json $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "pull copy from noauth to auth with a JSON --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src.json $NOAUTH_HTTPS//file_src $AUTH_HTTPS//file_dst$N
}

@test "pull copy from auth to noauth with a JSON --tpc-token-file without the source token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-dst.json $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "pull copy from noauth to auth with a JSON --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-dst.json $NOAUTH_HTTPS//file_src $AUTH_HTTPS//file_dst$N
}

@test "push copy from auth to noauth without a token fails" {
	run ! xrdcp -T push only $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "push copy from noauth to auth without a token fails" {
	run ! xrdcp -T push only $NOAUTH_HTTPS//file_src $AUTH_HTTPS//file_dst$N
}

@test "push copy from auth to noauth with --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-src $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "push copy from noauth to auth with --tpc-token-file without the destination token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-src $NOAUTH_HTTPS//file_src $AUTH_HTTPS//file_dst$N
}

@test "push copy from auth to noauth with --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "push copy from noauth to auth with --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-dst $NOAUTH_HTTPS//file_src $AUTH_HTTPS//file_dst$N
}

@test "push copy from auth to noauth with a JSON --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-src.json $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "push copy from noauth to auth with a JSON --tpc-token-file without the destination token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-src.json $NOAUTH_HTTPS//file_src $AUTH_HTTPS//file_dst$N
}

@test "push copy from auth to noauth with a JSON --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst.json $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "push copy from noauth to auth with a JSON --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-dst.json $NOAUTH_HTTPS//file_src $AUTH_HTTPS//file_dst$N
}

@test "pull copy from auth to noauth with an empty --tpc-token-file fails" {
	run ! xrdcp -T only --tpc-token-file token-file-empty $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "push copy from auth to noauth with an empty --tpc-token-file fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-empty $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "pull copy from auth to noauth with a JSON --tpc-token-file without the src and dst keys fails" {
	run ! xrdcp -T only --tpc-token-file token-file-invalid.json $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "push copy from auth to noauth with a JSON --tpc-token-file without the src and dst keys fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-invalid.json $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
}

@test "pull copy with a missing --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T only --tpc-token-file token-file-missing $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-missing'"
}

@test "push copy with a missing --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T push only --tpc-token-file token-file-missing $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-missing'"
}

@test "pull copy with an empty --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T only --tpc-token-file token-file-empty $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-empty'"
}

@test "push copy with an empty --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T push only --tpc-token-file token-file-empty $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-empty'"
}

@test "pull copy with a JSON --tpc-token-file without the src and dst keys reports a token file parse error" {
	run ! xrdcp -T only --tpc-token-file token-file-invalid.json $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-invalid.json'"
}

@test "push copy with a JSON --tpc-token-file without the src and dst keys reports a token file parse error" {
	run ! xrdcp -T push only --tpc-token-file token-file-invalid.json $AUTH_HTTPS//file_src $NOAUTH_HTTPS//file_dst$N
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-invalid.json'"
}

@test "pull copy from auth to noauth with the source encrypted and the destination plaintext succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-src $AUTH_HTTPS//file_src $NOAUTH_HTTP//file_dst$N
}

@test "pull copy from noauth to auth with the source plaintext and the destination encrypted succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-dst $NOAUTH_HTTP//file_src $AUTH_HTTPS//file_dst$N
}

@test "pull copy with --tpc-token-file sending the source token over an unencrypted source fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src $AUTH_HTTP//file_src $NOAUTH_HTTPS//file_dst$N
	assert_output --partial 'Invalid arguments: Refusing to send an Authorization header over an unencrypted http URL'
}

@test "pull copy with --tpc-token-file sending the destination token over an unencrypted destination fails" {
	run ! xrdcp -T only --tpc-token-file token-file-dst $AUTH_HTTPS//file_src $NOAUTH_HTTP//file_dst$N
	assert_output --partial 'Invalid arguments: Refusing to send an Authorization header over an unencrypted http URL'
}

@test "pull copy with --tpc-token-file sending both tokens over unencrypted endpoints fails" {
	run ! xrdcp -T only --tpc-token-file token-file-both $AUTH_HTTP//file_src $NOAUTH_HTTP//file_dst$N
	assert_output --partial 'Invalid arguments: Refusing to send an Authorization header over an unencrypted http URL'
}

@test "push copy with --tpc-token-file sending both tokens over unencrypted endpoints fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-both $AUTH_HTTP//file_src $NOAUTH_HTTP//file_dst$N
	assert_output --partial 'Invalid arguments: Refusing to send an Authorization header over an unencrypted http URL'
}
