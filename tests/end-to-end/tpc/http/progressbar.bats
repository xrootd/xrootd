#!/usr/bin/env bash

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPCERTFILE="$CREDENTIALS/ca.pem"

setup_file() {
	if [[ "$OSTYPE" == "darwin"* ]]; then
		skip "not running on macos"
	fi
	require_plugin libXrdHttpTPC
	start_server src server http http-tpc mirage-xattr.cfg
	start_server dst server http http-tpc mirage-xattr.cfg
	head -c 1048576 /dev/zero | xrdcp - "$SRC_HTTP//file_src"
}

teardown_file() {
	stop_servers
}

# Each test has its own destination files
setup() {
	cd "$BATS_TEST_TMPDIR"
	N=$BATS_TEST_NUMBER
}

@test "pull copy progress bar displays the transferred size and the total size" {
	run -0 script -e -q -c "xrdcp -T only $SRC_HTTP//file_src $DST_HTTP//file_dst$N"
	assert_output --partial '[1024kB/1024kB]'
}

@test "pull copy progress bar displays 100% when the copy completes" {
	run -0 script -e -q -c "xrdcp -T only $SRC_HTTP//file_src $DST_HTTP//file_dst$N"
	assert_output --partial '[100%][==================================================]'
}

@test "pull copy progress bar displays the transfer rate" {
	run -0 script -e -q -c "xrdcp -T only $SRC_HTTP//file_src $DST_HTTP//file_dst$N"
	assert_output --partial '[1024kB/s]'
}

@test "push copy progress bar displays the transferred size and the total size" {
	run -0 script -e -q -c "xrdcp -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst$N"
	assert_output --partial '[1024kB/1024kB]'
}

@test "push copy progress bar displays 100% when the copy completes" {
	run -0 script -e -q -c "xrdcp -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst$N"
	assert_output --partial '[100%][==================================================]'
}

@test "push copy progress bar displays the transfer rate" {
	run -0 script -e -q -c "xrdcp -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst$N"
	assert_output --partial '[1024kB/s]'
}

@test "pull copy with --nopbar succeeds" {
	run -0 xrdcp --nopbar -T only $SRC_HTTP//file_src $DST_HTTP//file_dst$N
}

@test "push copy with --nopbar succeeds" {
	run -0 xrdcp --nopbar -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst$N
}

@test "pull copy with -N succeeds" {
	run -0 xrdcp -N -T only $SRC_HTTP//file_src $DST_HTTP//file_dst$N
}

@test "pull copy with the legacy -np option succeeds" {
	run -0 xrdcp -np -T only $SRC_HTTP//file_src $DST_HTTP//file_dst$N
}
