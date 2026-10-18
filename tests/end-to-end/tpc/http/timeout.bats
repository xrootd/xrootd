#!/usr/bin/env bash

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPCERTFILE="$CREDENTIALS/ca.pem"

setup_file() {
	require_plugin libXrdHttpTPC
	start_server src server http http-tpc mirage-xattr.cfg
	start_server dst server http http-tpc mirage-xattr.cfg
	head -c 1 /dev/zero | xrdcp - "$SRC_HTTP//file_normal"
	xrdfs "$SRC_URL" truncate /file_normal 1048576
	head -c 1 /dev/zero | xrdcp - "$SRC_HTTP//file_slow"
	xrdfs "$SRC_URL" truncate /file_slow 26843545600
}

teardown_file() {
	stop_servers
}

# Each test has its own destination files
setup() {
	cd "$BATS_TEST_TMPDIR"
	N=$BATS_TEST_NUMBER
}

@test "pull copy succeeds when the transfer ends before the tpc timeout" {
	export XRD_CPTPCTIMEOUT=60
	run -0 xrdcp -T only $SRC_HTTP//file_normal $DST_HTTP//file_dst$N
}

@test "pull copy fails when the transfer reaches the tpc timeout" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T only $SRC_HTTP//file_slow $DST_HTTP//file_dst$N
}

@test "pull copy reports an expired operation when the tpc timeout is reached" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T only $SRC_HTTP//file_slow $DST_HTTP//file_dst$N
	assert_output --partial 'Operation expired: Operation timed out'
}

@test "push copy succeeds when the transfer ends before the tpc timeout" {
	export XRD_CPTPCTIMEOUT=60
	run -0 xrdcp -T push only $SRC_HTTP//file_normal $DST_HTTP//file_dst$N
}

@test "push copy fails when the transfer reaches the tpc timeout" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T push only $SRC_HTTP//file_slow $DST_HTTP//file_dst$N
}

@test "push copy reports an expired operation when the tpc timeout is reached" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T push only $SRC_HTTP//file_slow $DST_HTTP//file_dst$N
	assert_output --partial 'Operation expired: Operation timed out'
}
