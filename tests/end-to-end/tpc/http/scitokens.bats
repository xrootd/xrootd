#!/usr/bin/env bash

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPCERTFILE="$CREDENTIALS/ca.pem"

readonly SCOPE="storage.create:/ storage.read:/ storage.modify:/ storage.stage:/ storage.poll:/ storage.write:/ read:/ create:/ modify:/"

setup_file() {
	require_plugin libXrdHttpTPC
	require_plugin libXrdAccSciTokens
	start_issuer
	start_server src server http tls https-tpc scitokens
	start_server dst server http tls https-tpc scitokens
	echo 'source content' > "$SRC_DATA/file_src"
}

teardown_file() {
	stop_servers
}

# Each test has its own destination file
setup() {
	cd "$BATS_TEST_TMPDIR"
	N=$BATS_TEST_NUMBER

	src_token=$(make_token "$SCOPE")
	dst_token=$(make_token "$SCOPE")

	printf '%s\n%s\n' "$src_token" "$dst_token" > token-file
	printf '%s\n\n' "$src_token" > token-file-src
	printf '\n%s\n' "$dst_token" > token-file-dst

	jq -n --arg src "$src_token" --arg dst "$dst_token" '{ src: $src, dst: $dst }' > token-file.json
	jq -n --arg src "$src_token" '{ src: $src }' > token-file-src.json
	jq -n --arg dst "$dst_token" '{ dst: $dst }' > token-file-dst.json
}

@test "pull copy without a token fails" {
	run ! xrdcp -T only $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "pull copy with --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "pull copy with the BEARER_TOKEN_FILE variable holding both tokens succeeds" {
	BEARER_TOKEN_FILE=token-file \
	run -0 xrdcp -T only $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "pull copy with --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "pull copy with a JSON --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T only --tpc-token-file token-file.json $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "pull copy with the BEARER_TOKEN_FILE variable holding a JSON token file succeeds" {
	BEARER_TOKEN_FILE=token-file.json \
	run -0 xrdcp -T only $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "pull copy with a JSON --tpc-token-file without the destination token fails" {
	run ! xrdcp -T only --tpc-token-file token-file-src.json $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "push copy without a token fails" {
	run ! xrdcp -T push only $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "push copy with --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "push copy with the BEARER_TOKEN_FILE variable holding both tokens succeeds" {
	BEARER_TOKEN_FILE=token-file \
	run -0 xrdcp -T push only $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "push copy with --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "push copy with a JSON --tpc-token-file holding both tokens succeeds" {
	run -0 xrdcp -T push only --tpc-token-file token-file.json $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "push copy with the BEARER_TOKEN_FILE variable holding a JSON token file succeeds" {
	BEARER_TOKEN_FILE=token-file.json \
	run -0 xrdcp -T push only $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}

@test "push copy with a JSON --tpc-token-file without the source token fails" {
	run ! xrdcp -T push only --tpc-token-file token-file-dst.json $SRC_HTTPS//file_src $DST_HTTPS//file_dst$N
}
