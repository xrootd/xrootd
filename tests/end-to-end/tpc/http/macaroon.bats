#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug
export XRD_PLUGINCONFDIR="$BATS_TEST_DIRNAME"
export XRD_HTTPCERTFILE="$BATS_SUITE_TMPDIR/ca.pem"

readonly XROOTD_SRC="localhost:8478"
readonly XROOTD_DST="localhost:8479"

setup() {
	cd $BATS_TEST_TMPDIR

	openssl rand -base64 -out macaroon-secret 64

	sleep 0.5

	PORT=${XROOTD_SRC##*:} launch_xrootd macaroon.cfg xrootd_src
	PORT=${XROOTD_DST##*:} launch_xrootd macaroon.cfg xrootd_dst

	sleep 0.5

	src_token=$(curl -k --cert $BATS_SUITE_TMPDIR/client.crt --key $BATS_SUITE_TMPDIR/client.key -X POST -d '{ "caveats": [ "activity:READ_METADATA,UPDATE_METADATA,LIST,DOWNLOAD,UPLOAD,MANAGE,DELETE" ], "validity": "PT1H" }' -H 'Content-Type: application/macaroon-request' https://${XROOTD_SRC}/ | jq -r .macaroon)
	echo $src_token >  token-file
	dst_token=$(curl -k --cert $BATS_SUITE_TMPDIR/client.crt --key $BATS_SUITE_TMPDIR/client.key -X POST -d '{ "caveats": [ "activity:READ_METADATA,UPDATE_METADATA,LIST,DOWNLOAD,UPLOAD,MANAGE,DELETE" ], "validity": "PT1H" }' -H 'Content-Type: application/macaroon-request' https://${XROOTD_DST}/ | jq -r .macaroon)
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
	run ! xrdcp -T only https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "pull copy with --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "pull copy with the BEARER_TOKEN_FILE variable holding both tokens succeeds" {
	BEARER_TOKEN_FILE=token-file \
	run -0 xrdcp -T only https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "pull copy with --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "pull copy with a JSON --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file.json https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "pull copy with the BEARER_TOKEN_FILE variable holding a JSON token file succeeds" {
	BEARER_TOKEN_FILE=token-file.json \
	run -0 xrdcp -T only https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "pull copy with a JSON --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src.json https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "push copy without a token fails" {
	run ! xrdcp -T push only https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "push copy with --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "push copy with the BEARER_TOKEN_FILE variable holding both tokens succeeds" {
	BEARER_TOKEN_FILE=token-file \
	run -0 xrdcp -T push only https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "push copy with --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "push copy with a JSON --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file.json https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "push copy with the BEARER_TOKEN_FILE variable holding a JSON token file succeeds" {
	BEARER_TOKEN_FILE=token-file.json \
	run -0 xrdcp -T push only https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}

@test "push copy with a JSON --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst.json https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst
}
