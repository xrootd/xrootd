#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug
export XRD_PLUGINCONFDIR="$BATS_TEST_DIRNAME"

readonly XROOTD_SRC="localhost:8458"
readonly XROOTD_DST="localhost:8459"

setup() {
	cd $BATS_TEST_TMPDIR

	sleep 0.5

	PORT=${XROOTD_SRC##*:} launch_xrootd bigfiles.cfg xrootd_src
	PORT=${XROOTD_DST##*:} launch_xrootd bigfiles.cfg xrootd_dst

	sleep 0.5

	head -c 1 /dev/zero | xrdcp - http://${XROOTD_SRC}//file_1gb
	head -c 1 /dev/zero | xrdcp - http://${XROOTD_SRC}//file_10gb
	head -c 1 /dev/zero | xrdcp - http://${XROOTD_SRC}//file_25gb

	xrdfs root://${XROOTD_SRC}/ truncate /file_1gb 1073741824
	xrdfs root://${XROOTD_SRC}/ truncate /file_10gb 10737418240
	xrdfs root://${XROOTD_SRC}/ truncate /file_25gb 26843545600
}

teardown() {
	kill_pid_files
}

@test "pull copy transfers a 1GB file" {
	run -0 xrdcp -T only http://${XROOTD_SRC}//file_1gb http://${XROOTD_DST}//file_dst
}

@test "pull copy transfers a 10GB file" {
	run -0 xrdcp -T only http://${XROOTD_SRC}//file_10gb http://${XROOTD_DST}//file_dst
}

@test "pull copy transfers a 25GB file" {
	run -0 xrdcp -T only http://${XROOTD_SRC}//file_25gb http://${XROOTD_DST}//file_dst
}

@test "pull copy with 10 streams transfers a 1GB file" {
	skip "known bug: the pull copy with --streams for big files fails - event=MULTISTREAM_FAIL"

	run -0 xrdcp --streams 10 -T only http://${XROOTD_SRC}//file_1gb http://${XROOTD_DST}//file_dst
}

@test "pull copy with 10 streams transfers a 10GB file" {
	skip "known bug: the pull copy with --streams for big files fails - event=MULTISTREAM_FAIL"

	run -0 xrdcp --streams 10 -T only http://${XROOTD_SRC}//file_10gb http://${XROOTD_DST}//file_dst
}

@test "pull copy with 10 streams transfers a 25GB file" {
	skip "known bug: the pull copy with --streams for big files fails - event=MULTISTREAM_FAIL"

	run -0 xrdcp --streams 10 -T only http://${XROOTD_SRC}//file_25gb http://${XROOTD_DST}//file_dst
}

@test "push copy transfers a 1GB file" {
	run -0 xrdcp -T push only http://${XROOTD_SRC}//file_1gb http://${XROOTD_DST}//file_dst
}

@test "push copy transfers a 10GB file" {
	run -0 xrdcp -T push only http://${XROOTD_SRC}//file_10gb http://${XROOTD_DST}//file_dst
}

@test "push copy transfers a 25GB file" {
	run -0 xrdcp -T push only http://${XROOTD_SRC}//file_25gb http://${XROOTD_DST}//file_dst
}

@test "push copy with 10 streams transfers a 1GB file" {
	run -0 xrdcp --streams 10 -T push only http://${XROOTD_SRC}//file_1gb http://${XROOTD_DST}//file_dst
}

@test "push copy with 10 streams transfers a 10GB file" {
	run -0 xrdcp --streams 10 -T push only http://${XROOTD_SRC}//file_10gb http://${XROOTD_DST}//file_dst
}

@test "push copy with 10 streams transfers a 25GB file" {
	run -0 xrdcp --streams 10 -T push only http://${XROOTD_SRC}//file_25gb http://${XROOTD_DST}//file_dst
}
