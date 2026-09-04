#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash
load ../../helper/ssl.bash

export XRD_LOGLEVEL=Debug
export XRD_HTTPCERTFILE="$BATS_FILE_TMPDIR/ca.pem"

setup_file() {
	cd $BATS_FILE_TMPDIR

	generate_ca_files
	generate_host_files
}

setup() {
	cd $BATS_TEST_TMPDIR

	PORT=7094 launch_xrootd https.cfg xrootd_src
	PORT=7095 launch_xrootd https.cfg xrootd_dst

	sleep 0.5

	echo 'source content' > xrootd_src/file_src
	echo 'overwrite me!'  > xrootd_dst/file_dst_overwrite
}

teardown() {
	kill_pid_files
}

@test "pull copy writes the source content to the destination" {
	run -0 xrdcp -T only https://localhost:7094//file_src https://localhost:7095//file_dst
	assert_equal "$(cat xrootd_dst/file_dst)" 'source content'
}

@test "pull copy sends the Source header to the destination server" {
	run -0 xrdcp -T only https://localhost:7094//file_src https://localhost:7095//file_dst
	run -0 grep "Source: https://localhost:7094//file_src" xrootd_dst.log
}

@test "pull copy without --force fails when the destination exists" {
	run ! xrdcp -T only https://localhost:7094//file_src https://localhost:7095//file_dst_overwrite
}

@test "pull copy with --force succeeds when the destination exists" {
	run -0 xrdcp --force -T only https://localhost:7094//file_src https://localhost:7095//file_dst_overwrite
}

@test "pull copy with --force replaces the destination content" {
	run -0 xrdcp --force -T only https://localhost:7094//file_src https://localhost:7095//file_dst_overwrite
	assert_equal "$(cat xrootd_dst/file_dst_overwrite)" 'source content'
}

@test "push copy writes the source content to the destination" {
	run -0 xrdcp -T push only https://localhost:7094//file_src https://localhost:7095//file_dst
	assert_equal "$(cat xrootd_dst/file_dst)" 'source content'
}

@test "push copy sends the Destination header to the source server" {
	run -0 xrdcp -T push only https://localhost:7094//file_src https://localhost:7095//file_dst
	run -0 grep "Destination: https://localhost:7095//file_dst" xrootd_src.log
}

@test "push copy without --force fails when the destination exists" {
	skip "known bug: the push copy replaces the destination without --force"

	run ! xrdcp -T push only https://localhost:7094//file_src https://localhost:7095//file_dst_overwrite
}

@test "push copy with --force succeeds when the destination exists" {
	run -0 xrdcp --force -T push only https://localhost:7094//file_src https://localhost:7095//file_dst_overwrite
}

@test "push copy with --force replaces the destination content" {
	run -0 xrdcp --force -T push only https://localhost:7094//file_src https://localhost:7095//file_dst_overwrite
	assert_equal "$(cat xrootd_dst/file_dst_overwrite)" 'source content'
}
