#!/usr/bin/env bash

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPCERTFILE="$CREDENTIALS/ca.pem"

setup_file() {
	require_plugin libXrdHttpTPC
	start_server src server http http-tpc mirage-xattr.cfg
	start_server dst server http http-tpc mirage-xattr.cfg
	head -c 1 /dev/zero | xrdcp - "$SRC_HTTP//file_1gb"
	head -c 1 /dev/zero | xrdcp - "$SRC_HTTP//file_10gb"
	head -c 1 /dev/zero | xrdcp - "$SRC_HTTP//file_25gb"
	xrdfs "$SRC_URL" truncate /file_1gb 1073741824
	xrdfs "$SRC_URL" truncate /file_10gb 10737418240
	xrdfs "$SRC_URL" truncate /file_25gb 26843545600
}

teardown_file() {
	stop_servers
}

# Each test has its own destination files
setup() {
	cd "$BATS_TEST_TMPDIR"
	N=$BATS_TEST_NUMBER
}

@test "pull copy transfers a 1GB file" {
	run -0 xrdcp -T only $SRC_HTTP//file_1gb $DST_HTTP//file_dst$N
}

@test "pull copy transfers a 10GB file" {
	run -0 xrdcp -T only $SRC_HTTP//file_10gb $DST_HTTP//file_dst$N
}

@test "pull copy transfers a 25GB file" {
	run -0 xrdcp -T only $SRC_HTTP//file_25gb $DST_HTTP//file_dst$N
}

@test "pull copy with 10 streams transfers a 1GB file" {
	skip "known bug: the pull copy with --streams for big files fails - event=MULTISTREAM_FAIL"

	run -0 xrdcp --streams 10 -T only $SRC_HTTP//file_1gb $DST_HTTP//file_dst$N
}

@test "pull copy with 10 streams transfers a 10GB file" {
	skip "known bug: the pull copy with --streams for big files fails - event=MULTISTREAM_FAIL"

	run -0 xrdcp --streams 10 -T only $SRC_HTTP//file_10gb $DST_HTTP//file_dst$N
}

@test "pull copy with 10 streams transfers a 25GB file" {
	skip "known bug: the pull copy with --streams for big files fails - event=MULTISTREAM_FAIL"

	run -0 xrdcp --streams 10 -T only $SRC_HTTP//file_25gb $DST_HTTP//file_dst$N
}

@test "push copy transfers a 1GB file" {
	run -0 xrdcp -T push only $SRC_HTTP//file_1gb $DST_HTTP//file_dst$N
}

@test "push copy transfers a 10GB file" {
	run -0 xrdcp -T push only $SRC_HTTP//file_10gb $DST_HTTP//file_dst$N
}

@test "push copy transfers a 25GB file" {
	run -0 xrdcp -T push only $SRC_HTTP//file_25gb $DST_HTTP//file_dst$N
}

@test "push copy with 10 streams transfers a 1GB file" {
	run -0 xrdcp --streams 10 -T push only $SRC_HTTP//file_1gb $DST_HTTP//file_dst$N
}

@test "push copy with 10 streams transfers a 10GB file" {
	run -0 xrdcp --streams 10 -T push only $SRC_HTTP//file_10gb $DST_HTTP//file_dst$N
}

@test "push copy with 10 streams transfers a 25GB file" {
	run -0 xrdcp --streams 10 -T push only $SRC_HTTP//file_25gb $DST_HTTP//file_dst$N
}
