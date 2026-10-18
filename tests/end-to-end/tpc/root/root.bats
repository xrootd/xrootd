#!/usr/bin/env bash

load ../../setup/server.bash

setup_file() {
	start_server src server tpc
	start_server dst server tpc
}

teardown_file() {
	stop_servers
}

# Each test has its own source file, and a destination file to overwrite
setup() {
	N=$BATS_TEST_NUMBER
	echo 'source content' > "$SRC_DATA/file_src$N"
	echo 'overwrite me!'  > "$DST_DATA/file_dst_overwrite$N"
}

@test "pull copy writes the source content to the destination" {
	run -0 xrdcp -T only $SRC_URL//file_src$N $DST_URL//file_dst$N
	assert_equal "$(cat "$DST_DATA/file_dst$N")" 'source content'
}

@test "pull copy starts a TPC job on the destination server" {
	run -0 xrdcp -T only $SRC_URL//file_src$N $DST_URL//file_dst$N
	run -0 grep -F "copying xroot://localhost:$SRC_PORT//file_src$N" <(server_log dst)
}

@test "pull copy without --force fails when the destination exists" {
	run ! xrdcp -T only $SRC_URL//file_src$N $DST_URL//file_dst_overwrite$N
}

@test "pull copy with --force succeeds when the destination exists" {
	run -0 xrdcp --force -T only $SRC_URL//file_src$N $DST_URL//file_dst_overwrite$N
}

@test "pull copy with --force replaces the destination content" {
	run -0 xrdcp --force -T only $SRC_URL//file_src$N $DST_URL//file_dst_overwrite$N
	assert_equal "$(cat "$DST_DATA/file_dst_overwrite$N")" 'source content'
}
