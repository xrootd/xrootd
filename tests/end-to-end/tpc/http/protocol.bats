#!/usr/bin/env bash

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPCERTFILE="$CREDENTIALS/ca.pem"
export X509_CERT_FILE="$CREDENTIALS/ca.pem"

setup_file() {
	require_plugin libXrdHttpTPC
	start_server src server http tls https-tpc tpc roots.cfg
	start_server dst server http tls https-tpc tpc roots.cfg
	echo 'source content' > "$SRC_DATA/file_src"
}

teardown_file() {
	stop_servers
}

# Each test has its own destination files
setup() {
	cd "$BATS_TEST_TMPDIR"
	N=$BATS_TEST_NUMBER
	echo 'overwrite me!' > "$DST_DATA/file_dst_overwrite$N"
}

@test "third party copy from root to root succeeds" {
	run -0 xrdcp --force -T only $SRC_URL//file_src $DST_URL//file_dst_overwrite$N
}

@test "third party copy from http to http succeeds" {
	run -0 xrdcp --force -T only $SRC_HTTP//file_src $DST_HTTP//file_dst_overwrite$N
}

@test "third party copy from https to https succeeds" {
	run -0 xrdcp --force -T only $SRC_HTTPS//file_src $DST_HTTPS//file_dst_overwrite$N
}

@test "third party copy from http to https succeeds" {
	run -0 xrdcp --force -T only $SRC_HTTP//file_src $DST_HTTPS//file_dst_overwrite$N
}

@test "third party copy from https to http succeeds" {
	run -0 xrdcp --force -T only $SRC_HTTPS//file_src $DST_HTTP//file_dst_overwrite$N
}

@test "third party copy from http to root fails" {
	run ! xrdcp --force -T only $SRC_HTTP//file_src $DST_URL//file_dst_overwrite$N
}

@test "third party copy from root to http fails" {
	run ! xrdcp --force -T only $SRC_URL//file_src $DST_HTTP//file_dst_overwrite$N
}

@test "third party copy from https to root fails" {
	run ! xrdcp --force -T only $SRC_HTTPS//file_src $DST_URL//file_dst_overwrite$N
}

@test "third party copy from root to https fails" {
	run ! xrdcp --force -T only $SRC_URL//file_src $DST_HTTPS//file_dst_overwrite$N
}

@test "third party copy from http to root reports invalid arguments" {
	run ! xrdcp --force -T only $SRC_HTTP//file_src $DST_URL//file_dst_overwrite$N
	assert_output --partial 'Invalid arguments: Third party copy can only be done between http(s) protocols'
}

@test "third party copy from https to root reports invalid arguments" {
	run ! xrdcp --force -T only $SRC_HTTPS//file_src $DST_URL//file_dst_overwrite$N
	assert_output --partial 'Invalid arguments: Third party copy can only be done between http(s) protocols'
}

@test "third party copy from http to xroot reports invalid arguments" {
	run ! xrdcp --force -T only $SRC_HTTP//file_src xroot://localhost:$DST_PORT//file_dst_overwrite$N
	assert_output --partial 'Invalid arguments: Third party copy can only be done between http(s) protocols'
}

@test "third party copy from http to file reports invalid arguments" {
	run ! xrdcp --force -T only $SRC_HTTP//file_src file://localhost/$BATS_TEST_TMPDIR/file_dst
	assert_output --partial 'Invalid arguments: Third party copy can only be done between http(s) protocols'
}
