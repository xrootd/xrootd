#!/usr/bin/env bash

# a header the client refuses fails the transfer and never reaches the server

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

HOST=http://localhost:1097

setup() {
	# workdir as test tmp dir (all files are removed after execution)
	cd $BATS_TEST_TMPDIR
}

#
# arguments that cannot be parsed as a header
#

@test "--header without a name and value separator fails" {
	run ! xrdcp -H 'nocolon' $HOST//examplefile -
}

@test "--header without a name fails" {
	run ! xrdcp -H ': novalue' $HOST//examplefile -
}

@test "--header without a value fails" {
	run ! xrdcp -H 'X-Test-Header:' $HOST//examplefile -
}

@test "--header with a space in the name fails" {
	run ! xrdcp -H 'X Test: value' $HOST//examplefile -
}

@test "--header with a parenthesis in the name fails" {
	run ! xrdcp -H 'X(Test): value' $HOST//examplefile -
}

# A header that cannot be honoured must fail the transfer rather than quietly
# sending a request without it, which for a credential would look like a denial.
@test "an unusable header should not be silently dropped" {
	run xrdcp -H 'nocolon' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

#
# arguments that try to forge a request
#

@test "--header with a carriage return in the value fails" {
	run ! xrdcp -H $'X-Test-Header: a\rEvil: b' $HOST//examplefile -
}

@test "--header with a carriage return should not send the forged header" {
	run xrdcp -H $'X-Test-Header: a\rEvil: b' $HOST//examplefile -
	run ! grep -F -- 'got hdr line: Evil: b' header.log
}

# A newline in the argument separates headers rather than forging one, and each
# resulting header is validated in its own right.
@test "--header with a newline cannot smuggle a reserved header" {
	run ! xrdcp -H $'X-Test-Header: a\r\nHost: evil.example.com' $HOST//examplefile -
}

@test "--header with a newline should not send the smuggled header" {
	run xrdcp -H $'X-Test-Header: a\r\nHost: evil.example.com' $HOST//examplefile -
	run ! grep -F -- 'got hdr line: Host: evil.example.com' header.log
}

@test "--header with a newline should not send the header preceding it" {
	run xrdcp -H $'X-Test-Header: a\r\nHost: evil.example.com' $HOST//examplefile -
	run ! grep -F -- 'got hdr line: X-Test-Header: a' header.log
}

#
# headers the request builds for itself and cannot have overridden
#

@test "--header naming Host fails" {
	run ! xrdcp -H 'Host: evil.example.com' $HOST//examplefile -
}

@test "--header naming Transfer-Encoding fails" {
	run ! xrdcp -H 'Transfer-Encoding: chunked' $HOST//examplefile -
}

@test "--header naming Content-Length fails" {
	run ! xrdcp -H 'Content-Length: 0' $HOST//examplefile -
}

@test "--header naming Connection fails" {
	run ! xrdcp -H 'Connection: close' $HOST//examplefile -
}

@test "a reserved header should reject the whole command line" {
	run ! xrdcp -H 'X-Test-Header: e2e-value' -H 'Host: evil.example.com' \
		$HOST//examplefile -
}

@test "a rejected command line should not send the reserved header" {
	run xrdcp -H 'X-Test-Header: e2e-value' -H 'Host: evil.example.com' \
		$HOST//examplefile -
	run ! grep -F -- 'got hdr line: Host: evil.example.com' header.log
}

@test "a rejected command line should not send the accepted header either" {
	run xrdcp -H 'X-Test-Header: e2e-value' -H 'Host: evil.example.com' \
		$HOST//examplefile -
	run ! grep -F -- 'got hdr line: X-Test-Header: e2e-value' header.log
}

#
# the same headers aimed at the far server of a third party copy, which the
# TransferHeader prefix must not let through
#

@test "--header naming TransferHeaderHost fails" {
	run ! xrdcp -H 'TransferHeaderHost: evil.example.com' $HOST//examplefile -
}

@test "--header naming TransferHeaderTransfer-Encoding fails" {
	run ! xrdcp -H 'TransferHeaderTransfer-Encoding: chunked' $HOST//examplefile -
}

@test "--header naming TransferHeaderContent-Length fails" {
	run ! xrdcp -H 'TransferHeaderContent-Length: 0' $HOST//examplefile -
}

@test "--header naming TransferHeaderConnection fails" {
	run ! xrdcp -H 'TransferHeaderConnection: close' $HOST//examplefile -
}

@test "a reserved TransferHeader should reject the whole command line" {
	run ! xrdcp -H 'X-Test-Header: e2e-value' \
		-H 'TransferHeaderHost: evil.example.com' $HOST//examplefile -
}

@test "a rejected command line should not send the reserved TransferHeader" {
	run xrdcp -H 'X-Test-Header: e2e-value' \
		-H 'TransferHeaderHost: evil.example.com' $HOST//examplefile -
	run ! grep -F -- 'got hdr line: TransferHeaderHost: evil.example.com' header.log
}

@test "a command line rejected for a TransferHeader should not send the accepted header either" {
	run xrdcp -H 'X-Test-Header: e2e-value' \
		-H 'TransferHeaderHost: evil.example.com' $HOST//examplefile -
	run ! grep -F -- 'got hdr line: X-Test-Header: e2e-value' header.log
}
