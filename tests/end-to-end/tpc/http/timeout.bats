#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug
export XRD_PLUGINCONFDIR="$BATS_TEST_DIRNAME"

readonly XROOTD_SRC="localhost:8548"
readonly XROOTD_DST="localhost:8549"

setup() {
	cd $BATS_TEST_TMPDIR

	sleep 0.5

	PORT=${XROOTD_SRC##*:} launch_xrootd bigfiles.cfg xrootd_src
	PORT=${XROOTD_DST##*:} launch_xrootd bigfiles.cfg xrootd_dst

	sleep 0.5

	head -c 1 /dev/zero | xrdcp - http://${XROOTD_SRC}//file_normal
	xrdfs root://${XROOTD_SRC}/ truncate /file_normal 1048576

	head -c 1 /dev/zero | xrdcp - http://${XROOTD_SRC}//file_slow
	xrdfs root://${XROOTD_SRC}/ truncate /file_slow 26843545600
}

teardown() {
	kill_pid_files
}

@test "pull copy succeeds when the transfer ends before the tpc timeout" {
	export XRD_CPTPCTIMEOUT=60
	run -0 xrdcp -T only http://${XROOTD_SRC}//file_normal http://${XROOTD_DST}//file_dst
}

@test "pull copy fails when the transfer reaches the tpc timeout" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T only http://${XROOTD_SRC}//file_slow http://${XROOTD_DST}//file_dst
}

@test "pull copy reports an expired operation when the tpc timeout is reached" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T only http://${XROOTD_SRC}//file_slow http://${XROOTD_DST}//file_dst
	assert_output --partial 'Operation expired: Operation timed out'
}

@test "push copy succeeds when the transfer ends before the tpc timeout" {
	export XRD_CPTPCTIMEOUT=60
	run -0 xrdcp -T push only http://${XROOTD_SRC}//file_normal http://${XROOTD_DST}//file_dst
}

@test "push copy fails when the transfer reaches the tpc timeout" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T push only http://${XROOTD_SRC}//file_slow http://${XROOTD_DST}//file_dst
}

@test "push copy reports an expired operation when the tpc timeout is reached" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T push only http://${XROOTD_SRC}//file_slow http://${XROOTD_DST}//file_dst
	assert_output --partial 'Operation expired: Operation timed out'
}
