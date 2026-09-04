#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug

setup() {
	cd $BATS_TEST_TMPDIR

	PORT=7094 launch_xrootd root.cfg xrootd_src
	PORT=7095 launch_xrootd root.cfg xrootd_dst

	sleep 0.5

	echo 'source content' > xrootd_src/file_src
	echo 'overwrite me!'  > xrootd_dst/file_dst_overwrite
}

teardown() {
	kill_pid_files
}

@test "pull copy writes the source content to the destination" {
	run -0 xrdcp -T only root://localhost:7094//file_src root://localhost:7095//file_dst
	assert_equal "$(cat xrootd_dst/file_dst)" 'source content'
}

@test "pull copy starts a TPC job on the destination server" {
	run -0 xrdcp -T only root://localhost:7094//file_src root://localhost:7095//file_dst
	run -0 grep "copying xroot://localhost:7094//file_src" xrootd_dst.log
}

@test "pull copy without --force fails when the destination exists" {
	run ! xrdcp -T only root://localhost:7094//file_src root://localhost:7095//file_dst_overwrite
}

@test "pull copy with --force succeeds when the destination exists" {
	run -0 xrdcp --force -T only root://localhost:7094//file_src root://localhost:7095//file_dst_overwrite
}

@test "pull copy with --force replaces the destination content" {
	run -0 xrdcp --force -T only root://localhost:7094//file_src root://localhost:7095//file_dst_overwrite
	assert_equal "$(cat xrootd_dst/file_dst_overwrite)" 'source content'
}
