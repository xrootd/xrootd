#!/usr/bin/env bash

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPCERTFILE="$CREDENTIALS/ca.pem"

setup_file() {
	require_plugin libXrdHttpTPC
	start_server src server http http-tpc
	start_server dst server http http-tpc
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

@test "pull copy writes the source content to the destination" {
	run -0 xrdcp -T only $SRC_HTTP//file_src $DST_HTTP//file_dst$N
	assert_equal "$(cat $DST_DATA/file_dst$N)" 'source content'
}

@test "pull copy sends the Source header to the destination server" {
	run -0 xrdcp -T only $SRC_HTTP//file_src $DST_HTTP//file_dst$N
	run -0 grep "Source: $SRC_HTTP//file_src" <(server_log dst)
}

@test "pull copy without --force fails when the destination exists" {
	run ! xrdcp -T only $SRC_HTTP//file_src $DST_HTTP//file_dst_overwrite$N
}

@test "pull copy with --force succeeds when the destination exists" {
	run -0 xrdcp --force -T only $SRC_HTTP//file_src $DST_HTTP//file_dst_overwrite$N
}

@test "pull copy with --force replaces the destination content" {
	run -0 xrdcp --force -T only $SRC_HTTP//file_src $DST_HTTP//file_dst_overwrite$N
	assert_equal "$(cat $DST_DATA/file_dst_overwrite$N)" 'source content'
}

@test "push copy writes the source content to the destination" {
	run -0 xrdcp -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst$N
	assert_equal "$(cat $DST_DATA/file_dst$N)" 'source content'
}

@test "push copy sends the Destination header to the source server" {
	run -0 xrdcp -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst$N
	run -0 grep "Destination: $DST_HTTP//file_dst$N" <(server_log src)
}

@test "push copy forwards the Overwrite header to the destination" {
	run -0 xrdcp -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst$N
	run -0 grep "TransferHeaderOverwrite: F" <(server_log src)
}

@test "push copy without --force fails when the destination exists" {
	run ! xrdcp -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst_overwrite$N
}

@test "push copy without --force keeps the destination content" {
	run ! xrdcp -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst_overwrite$N
	assert_equal "$(cat $DST_DATA/file_dst_overwrite$N)" 'overwrite me!'
}

@test "push copy with --force succeeds when the destination exists" {
	run -0 xrdcp --force -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst_overwrite$N
}

@test "push copy with --force replaces the destination content" {
	run -0 xrdcp --force -T push only $SRC_HTTP//file_src $DST_HTTP//file_dst_overwrite$N
	assert_equal "$(cat $DST_DATA/file_dst_overwrite$N)" 'source content'
}
