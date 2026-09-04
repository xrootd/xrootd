#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash
load ../../helper/ssl.bash

export XRD_LOGLEVEL=Debug

setup_file() {
	cd $BATS_FILE_TMPDIR

	generate_ca_files
	generate_host_files
}

setup() {
	cd $BATS_TEST_TMPDIR

	generate_client_files $BATS_FILE_TMPDIR

	openssl rand -base64 -out macaroon-secret 64

	# xrootd_auth demands a token for a read and for a write.
	# xrootd_noauth demands no token.
	PORT=7094 launch_xrootd macaroon.cfg xrootd_auth
	PORT=7095 launch_xrootd plain.cfg    xrootd_noauth

	sleep 0.5

	auth_token=$(curl -k --cert client.pem --key client.key -X POST -d '{ "caveats": [ "activity:READ_METADATA,UPDATE_METADATA,LIST,DOWNLOAD,UPLOAD,MANAGE,DELETE" ], "validity": "PT1H" }' -H 'Content-Type: application/macaroon-request' https://localhost:7094/ | jq -r .macaroon)

	printf '%s\n\n' "$auth_token" > token-file-src
	printf '\n%s\n' "$auth_token" > token-file-dst
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
	run ! xrdcp -T only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy from noauth to auth without a token fails" {
	run ! xrdcp -T only http://localhost:7095//file_src http://localhost:7094//file_dst
}

@test "pull copy from auth to noauth with --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-src http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy from noauth to auth with --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src http://localhost:7095//file_src http://localhost:7094//file_dst
}

@test "pull copy from auth to noauth with --tpc-token-file without the source token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-dst http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy from noauth to auth with --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-dst http://localhost:7095//file_src http://localhost:7094//file_dst
}

@test "pull copy from auth to noauth with a JSON --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-src.json http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy from noauth to auth with a JSON --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src.json http://localhost:7095//file_src http://localhost:7094//file_dst
}

@test "pull copy from auth to noauth with a JSON --tpc-token-file without the source token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-dst.json http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy from noauth to auth with a JSON --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file-dst.json http://localhost:7095//file_src http://localhost:7094//file_dst
}

@test "push copy from auth to noauth without a token fails" {
	run ! xrdcp -T push only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy from noauth to auth without a token fails" {
	run ! xrdcp -T push only http://localhost:7095//file_src http://localhost:7094//file_dst
}

@test "push copy from auth to noauth with --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-src http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy from noauth to auth with --tpc-token-file without the destination token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-src http://localhost:7095//file_src http://localhost:7094//file_dst
}

@test "push copy from auth to noauth with --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy from noauth to auth with --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-dst http://localhost:7095//file_src http://localhost:7094//file_dst
}

@test "push copy from auth to noauth with a JSON --tpc-token-file without the destination token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-src.json http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy from noauth to auth with a JSON --tpc-token-file without the destination token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-src.json http://localhost:7095//file_src http://localhost:7094//file_dst
}

@test "push copy from auth to noauth with a JSON --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst.json http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy from noauth to auth with a JSON --tpc-token-file without the source token succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file-dst.json http://localhost:7095//file_src http://localhost:7094//file_dst
}

@test "pull copy from auth to noauth with an empty --tpc-token-file fails" {
	run ! xrdcp -T only --tpc-token-file token-file-empty http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy from auth to noauth with an empty --tpc-token-file fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-empty http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy from auth to noauth with a JSON --tpc-token-file without the src and dst keys fails" {
	run ! xrdcp -T only --tpc-token-file token-file-invalid.json http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy from auth to noauth with a JSON --tpc-token-file without the src and dst keys fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-invalid.json http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy with a missing --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T only --tpc-token-file token-file-missing http://localhost:7094//file_src http://localhost:7095//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-missing'"
}

@test "push copy with a missing --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T push only --tpc-token-file token-file-missing http://localhost:7094//file_src http://localhost:7095//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-missing'"
}

@test "pull copy with an empty --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T only --tpc-token-file token-file-empty http://localhost:7094//file_src http://localhost:7095//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-empty'"
}

@test "push copy with an empty --tpc-token-file reports a token file parse error" {
	run ! xrdcp -T push only --tpc-token-file token-file-empty http://localhost:7094//file_src http://localhost:7095//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-empty'"
}

@test "pull copy with a JSON --tpc-token-file without the src and dst keys reports a token file parse error" {
	run ! xrdcp -T only --tpc-token-file token-file-invalid.json http://localhost:7094//file_src http://localhost:7095//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-invalid.json'"
}

@test "push copy with a JSON --tpc-token-file without the src and dst keys reports a token file parse error" {
	run ! xrdcp -T push only --tpc-token-file token-file-invalid.json http://localhost:7094//file_src http://localhost:7095//file_dst
	assert_output --partial "Auth failed: Failed to parse the token file 'token-file-invalid.json'"
}
