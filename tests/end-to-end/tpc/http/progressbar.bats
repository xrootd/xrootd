#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug
export XRD_PLUGINCONFDIR="$BATS_TEST_DIRNAME"

readonly XROOTD_SRC="localhost:8508"
readonly XROOTD_DST="localhost:8509"

setup_file() {
	if [[ "$OSTYPE" == "darwin"* ]]; then
		skip "not running on macos"
	fi
}

setup() {
	cd $BATS_TEST_TMPDIR

	sleep 0.5

	PORT=${XROOTD_SRC##*:} launch_xrootd progressbar.cfg xrootd_src
	PORT=${XROOTD_DST##*:} launch_xrootd progressbar.cfg xrootd_dst

	sleep 0.5

	head -c 1048576 /dev/zero | xrdcp - http://${XROOTD_SRC}//file_src
}

teardown() {
	kill_pid_files
}

@test "pull copy progress bar displays the transferred size and the total size" {
	run -0 script -q -c "xrdcp -T only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst"
	assert_output --partial '[1024kB/1024kB]'
}

@test "pull copy progress bar displays 100% when the copy completes" {
	run -0 script -q -c "xrdcp -T only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst"
	assert_output --partial '[100%][==================================================]'
}

@test "pull copy progress bar displays the transfer rate" {
	run -0 script -q -c "xrdcp -T only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst"
	assert_output --partial '[1024kB/s]'
}

@test "push copy progress bar displays the transferred size and the total size" {
	run -0 script -q -c "xrdcp -T push only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst"
	assert_output --partial '[1024kB/1024kB]'
}

@test "push copy progress bar displays 100% when the copy completes" {
	run -0 script -q -c "xrdcp -T push only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst"
	assert_output --partial '[100%][==================================================]'
}

@test "push copy progress bar displays the transfer rate" {
	run -0 script -q -c "xrdcp -T push only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst"
	assert_output --partial '[1024kB/s]'
}

@test "pull copy with --nopbar succeeds" {
	run -0 xrdcp --nopbar -T only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst
}

@test "push copy with --nopbar succeeds" {
	run -0 xrdcp --nopbar -T push only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst
}

@test "pull copy with -N succeeds" {
	run -0 xrdcp -N -T only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst
}

@test "pull copy with the legacy -np option succeeds" {
	run -0 xrdcp -np -T only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst
}
