#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

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

	PORT=7094 launch_xrootd macaroon.cfg xrootd_src
	PORT=7095 launch_xrootd macaroon.cfg xrootd_dst

	sleep 0.5

	src_token=$(curl -k --cert client.pem --key client.key -X POST -d '{ "caveats": [ "activity:READ_METADATA,UPDATE_METADATA,LIST,DOWNLOAD,UPLOAD,MANAGE,DELETE" ], "validity": "PT1H" }' -H 'Content-Type: application/macaroon-request' https://localhost:7094/ | jq -r .macaroon)
	echo $src_token >  token-file
	dst_token=$(curl -k --cert client.pem --key client.key -X POST -d '{ "caveats": [ "activity:READ_METADATA,UPDATE_METADATA,LIST,DOWNLOAD,UPLOAD,MANAGE,DELETE" ], "validity": "PT1H" }' -H 'Content-Type: application/macaroon-request' https://localhost:7095/ | jq -r .macaroon)
	echo $dst_token >> token-file

	printf '%s\n\n' "$src_token" > token-file-src
	printf '\n%s\n' "$dst_token" > token-file-dst

	jq -n --arg src "$src_token" --arg dst "$dst_token" '{ src: $src, dst: $dst }' > token-file.json
	jq -n --arg src "$src_token" '{ src: $src }' > token-file-src.json
	jq -n --arg dst "$dst_token" '{ dst: $dst }' > token-file-dst.json

	echo 'source content' > xrootd_src/file_src
}

teardown() {
	kill_pid_files
}

@test "pull copy without a token fails" {
	run ! xrdcp -T only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy with --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy with the BEARER_TOKEN_FILE variable holding both tokens succeeds" {
	BEARER_TOKEN_FILE=token-file \
	run -0 xrdcp -T only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy with --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy with a JSON --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file.json http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy with the BEARER_TOKEN_FILE variable holding a JSON token file succeeds" {
	BEARER_TOKEN_FILE=token-file.json \
	run -0 xrdcp -T only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy with a JSON --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src.json http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy without a token fails" {
	run ! xrdcp -T push only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy with --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy with the BEARER_TOKEN_FILE variable holding both tokens succeeds" {
	BEARER_TOKEN_FILE=token-file \
	run -0 xrdcp -T push only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy with --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy with a JSON --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file.json http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy with the BEARER_TOKEN_FILE variable holding a JSON token file succeeds" {
	BEARER_TOKEN_FILE=token-file.json \
	run -0 xrdcp -T push only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy with a JSON --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst.json http://localhost:7094//file_src http://localhost:7095//file_dst
}
