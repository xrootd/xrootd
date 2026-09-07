#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug
export XRD_PLUGINCONFDIR="$BATS_TEST_DIRNAME"
export XRD_HTTPCERTFILE="$BATS_FILE_TMPDIR/ca.pem"

readonly XROOTD_SRC="localhost:8488"
readonly XROOTD_DST="localhost:8489"

setup_file() {
	if [[ -z "${IAM_OAUTH_ISSUER}" || -z "${IAM_OAUTH_CLIENT_ID}" || -z "${IAM_OAUTH_CLIENT_SECRET}" ]]; then
		skip "Missing issuer, client id or secret"
	fi

	cd $BATS_FILE_TMPDIR

	sed "s|issuer =.*|issuer = ${IAM_OAUTH_ISSUER}|" ${BATS_TEST_DIRNAME}/oauth-module.cfg > oauth-module.cfg

	endpoint=$(curl -s "${IAM_OAUTH_ISSUER}/.well-known/openid-configuration" | jq -r .token_endpoint)

	scope='storage.read:/ storage.modify:/ storage.create:/ storage.stage:/'
	export TOKEN=$(curl -s -X POST --user "${IAM_OAUTH_CLIENT_ID}:${IAM_OAUTH_CLIENT_SECRET}" \
		-H "Content-Type: application/x-www-form-urlencoded" \
		--data "grant_type=client_credentials&scope=${scope}" \
		${endpoint} | jq -r .access_token)
}

setup() {
	cd $BATS_TEST_TMPDIR

	sleep 0.5

	PORT=${XROOTD_SRC##*:} launch_xrootd oauth.cfg xrootd_src
	PORT=${XROOTD_DST##*:} launch_xrootd oauth.cfg xrootd_dst

	sleep 0.5

	echo 'source content' > xrootd_src/file_src

	echo $TOKEN >  token-file
	echo $TOKEN >> token-file

	printf '%s\n\n' "$TOKEN" > token-file-src
	printf '\n%s\n' "$TOKEN" > token-file-dst

	jq -n --arg src "$TOKEN" --arg dst "$TOKEN" '{ src: $src, dst: $dst }' > token-file.json
	jq -n --arg src "$TOKEN" '{ src: $src }' > token-file-src.json
	jq -n --arg dst "$TOKEN" '{ dst: $dst }' > token-file-dst.json
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
