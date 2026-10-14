#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug
export XRD_PLUGINCONFDIR="$BATS_TEST_DIRNAME"

export XRD_HTTPCERTFILE="$BATS_SUITE_TMPDIR/ca.pem"

readonly XROOTD_AUTH="localhost:8558"
readonly XROOTD_NOAUTH="localhost:8559"

setup() {
	cd $BATS_TEST_TMPDIR

	openssl rand -base64 -out macaroon-secret 64

	sleep 0.5

	# xrootd_auth demands a token for a read and for a write, over http or https.
	# xrootd_noauth demands no token, over http or https.
	PORT=${XROOTD_AUTH##*:} launch_xrootd macaroon.cfg xrootd_auth
	PORT=${XROOTD_NOAUTH##*:} launch_xrootd https.cfg    xrootd_noauth

	sleep 0.5

	auth_token=$(curl -k --cert $BATS_SUITE_TMPDIR/client.crt --key $BATS_SUITE_TMPDIR/client.key -X POST -d '{ "caveats": [ "activity:READ_METADATA,UPDATE_METADATA,LIST,DOWNLOAD,UPLOAD,MANAGE,DELETE" ], "validity": "PT1H" }' -H 'Content-Type: application/macaroon-request' https://${XROOTD_AUTH}/ | jq -r .macaroon)

	printf '%s\n\n' "$auth_token" > token-file-src
	printf '\n%s\n' "$auth_token" > token-file-dst
	printf '%s\n%s\n' "$auth_token" "$auth_token" > token-file-both
	printf '\n\n' > token-file-empty

	jq -n --arg src "$auth_token" '{ src: $src }' > token-file-src.json
	jq -n --arg dst "$auth_token" '{ dst: $dst }' > token-file-dst.json
	echo '{ "token": "invalid" }' > token-file-invalid.json

	echo 'source content' > xrootd_auth/file_src
	echo 'source content' > xrootd_noauth/file_src
}

teardown() {
	kill_pid_files
}

@test "pull copy from auth to noauth without a token fails" {
	run ! xrdcp -T only https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "pull copy from noauth to auth without a token fails" {
	run ! xrdcp -T only https://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "pull copy from auth to noauth with --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-src https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "pull copy from noauth to auth with --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src https://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "pull copy from auth to noauth with --tpc-token-file without the source token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-dst https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "pull copy from noauth to auth with --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-dst https://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "pull copy from auth to noauth with a JSON --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-src.json https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "pull copy from noauth to auth with a JSON --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src.json https://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "pull copy from auth to noauth with a JSON --tpc-token-file without the source token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-dst.json https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "pull copy from noauth to auth with a JSON --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-dst.json https://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "push copy from auth to noauth without a token fails" {
	run ! xrdcp -T push only https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "push copy from noauth to auth without a token fails" {
	run ! xrdcp -T push only https://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "push copy from auth to noauth with --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-src https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "push copy from noauth to auth with --tpc-token-file without the destination token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-src https://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "push copy from auth to noauth with --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "push copy from noauth to auth with --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-dst https://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "push copy from auth to noauth with a JSON --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-src.json https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "push copy from noauth to auth with a JSON --tpc-token-file without the destination token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-src.json https://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "push copy from auth to noauth with a JSON --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst.json https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "push copy from noauth to auth with a JSON --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-dst.json https://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "pull copy from auth to noauth with an empty --tpc-token-file fails" {
	run ! xrdcp -T only --tpc-token-file token-file-empty https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "push copy from auth to noauth with an empty --tpc-token-file fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-empty https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "pull copy from auth to noauth with a JSON --tpc-token-file without the src and dst keys fails" {
	run ! xrdcp -T only --tpc-token-file token-file-invalid.json https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "push copy from auth to noauth with a JSON --tpc-token-file without the src and dst keys fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-invalid.json https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
}

@test "pull copy with a missing --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T only --tpc-token-file token-file-missing https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-missing'"
}

@test "push copy with a missing --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T push only --tpc-token-file token-file-missing https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-missing'"
}

@test "pull copy with an empty --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T only --tpc-token-file token-file-empty https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-empty'"
}

@test "push copy with an empty --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T push only --tpc-token-file token-file-empty https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-empty'"
}

@test "pull copy with a JSON --tpc-token-file without the src and dst keys reports a token file parse error" {
	run ! xrdcp -T only --tpc-token-file token-file-invalid.json https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-invalid.json'"
}

@test "push copy with a JSON --tpc-token-file without the src and dst keys reports a token file parse error" {
	run ! xrdcp -T push only --tpc-token-file token-file-invalid.json https://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-invalid.json'"
}

@test "pull copy from auth to noauth with the source encrypted and the destination plaintext succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-src https://${XROOTD_AUTH}//file_src http://${XROOTD_NOAUTH}//file_dst
}

@test "pull copy from noauth to auth with the source plaintext and the destination encrypted succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-dst http://${XROOTD_NOAUTH}//file_src https://${XROOTD_AUTH}//file_dst
}

@test "pull copy with --tpc-token-file sending the source token over an unencrypted source fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src http://${XROOTD_AUTH}//file_src https://${XROOTD_NOAUTH}//file_dst
	assert_output --partial 'Invalid arguments: Refusing to send an Authorization header over an unencrypted http URL'
}

@test "pull copy with --tpc-token-file sending the destination token over an unencrypted destination fails" {
	run ! xrdcp -T only --tpc-token-file token-file-dst https://${XROOTD_AUTH}//file_src http://${XROOTD_NOAUTH}//file_dst
	assert_output --partial 'Invalid arguments: Refusing to send an Authorization header over an unencrypted http URL'
}

@test "pull copy with --tpc-token-file sending both tokens over unencrypted endpoints fails" {
	run ! xrdcp -T only --tpc-token-file token-file-both http://${XROOTD_AUTH}//file_src http://${XROOTD_NOAUTH}//file_dst
	assert_output --partial 'Invalid arguments: Refusing to send an Authorization header over an unencrypted http URL'
}

@test "push copy with --tpc-token-file sending both tokens over unencrypted endpoints fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-both http://${XROOTD_AUTH}//file_src http://${XROOTD_NOAUTH}//file_dst
	assert_output --partial 'Invalid arguments: Refusing to send an Authorization header over an unencrypted http URL'
}
