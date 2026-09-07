#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug

readonly XROOTD_SRC="localhost:8528"
readonly XROOTD_DST="localhost:8529"

setup() {
	cd $BATS_TEST_TMPDIR

	sleep 0.5

	PORT=${XROOTD_SRC##*:} launch_xrootd root.cfg xrootd_src
	PORT=${XROOTD_DST##*:} launch_xrootd root.cfg xrootd_dst

	sleep 0.5

	echo 'source content' > xrootd_src/file_src
	echo 'overwrite me!'  > xrootd_dst/file_dst_overwrite
}

teardown() {
	kill_pid_files
}

@test "pull copy writes the source content to the destination" {
	run -0 xrdcp -T only root://${XROOTD_SRC}//file_src root://${XROOTD_DST}//file_dst
	assert_equal "$(cat xrootd_dst/file_dst)" 'source content'
}

@test "pull copy starts a TPC job on the destination server" {
	run -0 xrdcp -T only root://${XROOTD_SRC}//file_src root://${XROOTD_DST}//file_dst
	run -0 grep "copying xroot://${XROOTD_SRC}//file_src" xrootd_dst.log
}

@test "pull copy without --force fails when the destination exists" {
	run ! xrdcp -T only root://${XROOTD_SRC}//file_src root://${XROOTD_DST}//file_dst_overwrite
}

@test "pull copy with --force succeeds when the destination exists" {
	run -0 xrdcp --force -T only root://${XROOTD_SRC}//file_src root://${XROOTD_DST}//file_dst_overwrite
}

@test "pull copy with --force replaces the destination content" {
	run -0 xrdcp --force -T only root://${XROOTD_SRC}//file_src root://${XROOTD_DST}//file_dst_overwrite
	assert_equal "$(cat xrootd_dst/file_dst_overwrite)" 'source content'
}
