#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug

setup() {
	cd $BATS_TEST_TMPDIR

	PORT=7094 launch_xrootd bigfiles.cfg xrootd_src
	PORT=7095 launch_xrootd bigfiles.cfg xrootd_dst

	sleep 0.5

	head -c 1 /dev/zero | xrdcp - http://localhost:7094//file_normal
	xrdfs root://localhost:7094/ truncate /file_normal 1048576

	head -c 1 /dev/zero | xrdcp - http://localhost:7094//file_slow
	xrdfs root://localhost:7094/ truncate /file_slow 26843545600
}

teardown() {
	kill_pid_files
}

@test "pull copy succeeds when the transfer ends before the tpc timeout" {
	export XRD_CPTPCTIMEOUT=60
	run -0 xrdcp -T only http://localhost:7094//file_normal http://localhost:7095//file_dst
}

@test "pull copy fails when the transfer reaches the tpc timeout" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T only http://localhost:7094//file_slow http://localhost:7095//file_dst
}

@test "pull copy reports an expired operation when the tpc timeout is reached" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T only http://localhost:7094//file_slow http://localhost:7095//file_dst
	assert_output --partial 'Operation expired: Operation timed out'
}

@test "push copy succeeds when the transfer ends before the tpc timeout" {
	export XRD_CPTPCTIMEOUT=60
	run -0 xrdcp -T push only http://localhost:7094//file_normal http://localhost:7095//file_dst
}

@test "push copy fails when the transfer reaches the tpc timeout" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T push only http://localhost:7094//file_slow http://localhost:7095//file_dst
}

@test "push copy reports an expired operation when the tpc timeout is reached" {
	export XRD_CPTPCTIMEOUT=2
	run ! xrdcp -T push only http://localhost:7094//file_slow http://localhost:7095//file_dst
	assert_output --partial 'Operation expired: Operation timed out'
}
