#!/usr/bin/env bash

# a header given on the command line reaches the server

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

HOST=http://localhost:1097
ROOT=root://localhost:1097

setup() {
	# workdir as test tmp dir (all files are removed after execution)
	cd $BATS_TEST_TMPDIR

	launch_xrootd header.cfg header

	sleep 0.5

	echo 'example file!' | xrdcp - $ROOT//examplefile
}

teardown() {
    kill_pid_files
}

#
# injecting a header on download
#

@test "download with an injected header should succeed" {
	run -0 xrdcp -H 'X-Test-Header: e2e-value' $HOST//examplefile -
}

@test "download with an injected header should return the file contents" {
	run xrdcp -H 'X-Test-Header: e2e-value' $HOST//examplefile -
	assert_output 'example file!'
}

@test "download with an injected header should send the header" {
	run xrdcp -H 'X-Test-Header: e2e-value' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Test-Header: e2e-value' header.log
}

@test "long form --header should succeed" {
	run -0 xrdcp --header 'X-Test-Header: e2e-value' $HOST//examplefile -
}

@test "long form --header should send the header" {
	run xrdcp --header 'X-Test-Header: e2e-value' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Test-Header: e2e-value' header.log
}

@test "repeating --header should send the first header" {
	run xrdcp -H 'X-First: one' -H 'X-Second: two' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-First: one' header.log
}

@test "repeating --header should send the second header" {
	run xrdcp -H 'X-First: one' -H 'X-Second: two' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Second: two' header.log
}

@test "a header value containing spaces and colons should be sent verbatim" {
	run xrdcp -H 'X-Test-Header: a:b c:d' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Test-Header: a:b c:d' header.log
}

# A user who holds a token has to be able to present it, so Authorization is the
# caller's to set. The server log holds the name but redacts the credential, so
# the tests below assert the name alone.

@test "download with an injected Authorization header should succeed" {
	run -0 xrdcp -H 'Authorization: Bearer e2e-token' $HOST//examplefile -
}

@test "download with an injected Authorization header should send the header" {
	run xrdcp -H 'Authorization: Bearer e2e-token' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: Authorization:' header.log
}

@test "download with an injected TransferHeaderAuthorization header should succeed" {
	run -0 xrdcp -H 'TransferHeaderAuthorization: Bearer e2e-token' $HOST//examplefile -
}

@test "download with an injected TransferHeaderAuthorization header should send the header" {
	run xrdcp -H 'TransferHeaderAuthorization: Bearer e2e-token' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: TransferHeaderAuthorization:' header.log
}

#
# injecting a header on upload
#

@test "upload with an injected header should succeed" {
	echo 'uploaded!' > localfile
	run -0 xrdcp -H 'X-Test-Header: e2e-value' localfile $HOST//uploadedfile
}

@test "upload with an injected header should send the header" {
	echo 'uploaded!' > localfile
	run xrdcp -H 'X-Test-Header: e2e-value' localfile $HOST//uploadedfile
	run -0 grep -F -- 'got hdr line: X-Test-Header: e2e-value' header.log
}

@test "upload with an injected header should store the file contents" {
	echo 'uploaded!' > localfile
	run xrdcp -H 'X-Test-Header: e2e-value' localfile $HOST//uploadedfile
	run xrdcp $ROOT//uploadedfile -
	assert_output 'uploaded!'
}

#
# a transfer without --header, as the baseline the tests above are read against
#

@test "download without --header should return the file contents" {
	run xrdcp $HOST//examplefile -
	assert_output 'example file!'
}

@test "download without --header should not send the header" {
	# the download has to succeed, otherwise the header is missing from the log
	# only because no request was ever made
	run -0 xrdcp $HOST//examplefile -
	run ! grep -F -- 'got hdr line: X-Test-Header' header.log
}
