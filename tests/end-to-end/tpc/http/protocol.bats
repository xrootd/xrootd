#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug
export XRD_PLUGINCONFDIR="$BATS_TEST_DIRNAME"

export XRD_HTTPCERTFILE="$BATS_SUITE_TMPDIR/ca.pem"
export X509_CERT_FILE="$BATS_SUITE_TMPDIR/ca.pem"

readonly XROOTD_SRC="localhost:8518"
readonly XROOTD_DST="localhost:8519"

setup() {
	cd $BATS_TEST_TMPDIR

	sleep 0.5

	PORT=${XROOTD_SRC##*:} launch_xrootd protocol.cfg xrootd_src
	PORT=${XROOTD_DST##*:} launch_xrootd protocol.cfg xrootd_dst

	sleep 0.5

	echo 'source content' > xrootd_src/file_src
	echo 'overwrite me!'  > xrootd_dst/file_dst_overwrite
}

teardown() {
	kill_pid_files
}

@test "third party copy from root to root succeeds" {
	run -0 xrdcp --force -T only root://${XROOTD_SRC}//file_src root://${XROOTD_DST}//file_dst_overwrite
}

@test "third party copy from http to http succeeds" {
	run -0 xrdcp --force -T only http://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst_overwrite
}

@test "third party copy from https to https succeeds" {
	run -0 xrdcp --force -T only https://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst_overwrite
}

@test "third party copy from http to https succeeds" {
	run -0 xrdcp --force -T only http://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst_overwrite
}

@test "third party copy from https to http succeeds" {
	run -0 xrdcp --force -T only https://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst_overwrite
}

@test "third party copy from http to root fails" {
	run ! xrdcp --force -T only http://${XROOTD_SRC}//file_src root://${XROOTD_DST}//file_dst_overwrite
}

@test "third party copy from root to http fails" {
	run ! xrdcp --force -T only root://${XROOTD_SRC}//file_src http://${XROOTD_DST}//file_dst_overwrite
}

@test "third party copy from https to root fails" {
	run ! xrdcp --force -T only https://${XROOTD_SRC}//file_src root://${XROOTD_DST}//file_dst_overwrite
}

@test "third party copy from root to https fails" {
	run ! xrdcp --force -T only root://${XROOTD_SRC}//file_src https://${XROOTD_DST}//file_dst_overwrite
}

@test "third party copy from http to root reports invalid arguments" {
	run ! xrdcp --force -T only http://${XROOTD_SRC}//file_src root://${XROOTD_DST}//file_dst_overwrite
	assert_output --partial 'Invalid arguments: Third party copy can only be done between http(s) protocols'
}

@test "third party copy from https to root reports invalid arguments" {
	run ! xrdcp --force -T only https://${XROOTD_SRC}//file_src root://${XROOTD_DST}//file_dst_overwrite
	assert_output --partial 'Invalid arguments: Third party copy can only be done between http(s) protocols'
}

@test "third party copy from http to xroot reports invalid arguments" {
	run ! xrdcp --force -T only http://${XROOTD_SRC}//file_src xroot://${XROOTD_DST}//file_dst_overwrite
	assert_output --partial 'Invalid arguments: Third party copy can only be done between http(s) protocols'
}

@test "third party copy from http to file reports invalid arguments" {
	run ! xrdcp --force -T only http://${XROOTD_SRC}//file_src file://localhost/$BATS_TEST_TMPDIR/file_dst
	assert_output --partial 'Invalid arguments: Third party copy can only be done between http(s) protocols'
}
