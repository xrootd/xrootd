#!/usr/bin/env bash

# A PUT with 'Overwrite: F' must not replace an existing file. The HTTP TPC
# push mode forwards this header to the destination when --force is not set.

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

HOST=http://localhost:9881

setup() {
	cd $BATS_TEST_TMPDIR

	PORT=9881 launch_xrootd put.cfg put

	echo 'original' > original
	echo 'replacement' > replacement

	curl --fail --silent --show-error -T original $HOST/file
}

teardown() {
	kill_pid_files
}

@test "PUT without an Overwrite header replaces an existing file" {
	run -0 curl --fail --silent --show-error -T replacement $HOST/file
}

@test "PUT with 'Overwrite: T' replaces an existing file" {
	curl --fail --silent --show-error -H 'Overwrite: T' -T replacement $HOST/file
	run -0 curl --fail --silent --show-error $HOST/file
	assert_output 'replacement'
}

@test "PUT with 'Overwrite: F' fails with 409 when the file exists" {
	run -0 curl --silent --output /dev/null --write-out '%{http_code}' -H 'Overwrite: F' -T replacement $HOST/file
	assert_output '409'
}

@test "PUT with 'Overwrite: F' keeps the content of an existing file" {
	curl --silent --output /dev/null -H 'Overwrite: F' -T replacement $HOST/file || true
	run -0 curl --fail --silent --show-error $HOST/file
	assert_output 'original'
}

@test "PUT with 'Overwrite: F' creates a new file" {
	run -0 curl --fail --silent --show-error -H 'Overwrite: F' -T replacement $HOST/newfile
}
