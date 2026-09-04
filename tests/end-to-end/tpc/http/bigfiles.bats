#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug

setup() {
	cd $BATS_TEST_TMPDIR

	PORT=7094 launch_xrootd bigfiles.cfg xrootd_src
	PORT=7095 launch_xrootd bigfiles.cfg xrootd_dst

	sleep 0.5

	head -c 1 /dev/zero | xrdcp - http://localhost:7094//file_src
}

teardown() {
	kill_pid_files
}

@test "pull copy transfers a 1GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 1073741824

	run -0 xrdcp -T only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy transfers a 10GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 10737418240

	run -0 xrdcp -T only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy transfers a 25GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 26843545600

	run -0 xrdcp -T only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy with 10 streams transfers a 1GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 1073741824

	run -0 xrdcp --streams 10 -T only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy with 10 streams transfers a 10GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 10737418240

	run -0 xrdcp --streams 10 -T only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "pull copy with 10 streams transfers a 25GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 26843545600

	run -0 xrdcp --streams 10 -T only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy transfers a 1GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 1073741824

	run -0 xrdcp -T push only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy transfers a 10GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 10737418240

	run -0 xrdcp -T push only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy transfers a 25GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 26843545600

	run -0 xrdcp -T push only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy with 10 streams transfers a 1GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 1073741824

	run -0 xrdcp --streams 10 -T push only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy with 10 streams transfers a 10GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 10737418240

	run -0 xrdcp --streams 10 -T push only http://localhost:7094//file_src http://localhost:7095//file_dst
}

@test "push copy with 10 streams transfers a 25GB file" {
	xrdfs root://localhost:7094/ truncate /file_src 26843545600

	run -0 xrdcp --streams 10 -T push only http://localhost:7094//file_src http://localhost:7095//file_dst
}
