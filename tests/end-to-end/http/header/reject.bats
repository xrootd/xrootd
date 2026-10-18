#!/usr/bin/env bash

# a header the client refuses fails the transfer and never reaches the server

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"

# The server logs each header of a request which it gets. The file exists,
# thus a transfer fails only when the client refuses the header, and the
# client then reports "Invalid header requested".
setup_file() {
	require_plugin libXrdHttp
	start_server http.cfg trace-http.cfg
	echo 'example file!' > "$SERVER_DATA/examplefile"
}

teardown_file() {
	stop_servers
}

setup() {
	cd "$BATS_TEST_TMPDIR"
	HOST=$SERVER_HTTP
}

# refuted <header line>: the server log exists and does not have the header
refuted() {
	run -0 server_log server
	refute_output --partial "got hdr line: $1"
}

# the control for the checks of the log below: a header which the client
# accepts is in the log of the server
@test "the server logs a header which the client accepts" {
	run -0 xrdcp -H 'X-Test-Header: e2e-value' $HOST//examplefile -
	run -0 server_log server
	assert_output --partial 'got hdr line: X-Test-Header: e2e-value'
}

#
# arguments that cannot be parsed as a header
#

@test "--header without a name and value separator fails" {
	run ! xrdcp -H 'nocolon' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header without a name fails" {
	run ! xrdcp -H ': novalue' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header without a value fails" {
	run ! xrdcp -H 'X-Test-Header:' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header with a space in the name fails" {
	run ! xrdcp -H 'X Test: value' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header with a parenthesis in the name fails" {
	run ! xrdcp -H 'X(Test): value' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
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
	assert_output --partial 'Invalid header requested'
}

@test "--header with a carriage return should not send the forged header" {
	run xrdcp -H $'X-Test-Header: a\rEvil: b' $HOST//examplefile -
	refuted 'Evil: b'
}

# A newline in the argument separates headers rather than forging one, and each
# resulting header is validated in its own right.
@test "--header with a newline cannot smuggle a reserved header" {
	run ! xrdcp -H $'X-Test-Header: a\r\nHost: evil.example.com' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header with a newline should not send the smuggled header" {
	run xrdcp -H $'X-Test-Header: a\r\nHost: evil.example.com' $HOST//examplefile -
	refuted 'Host: evil.example.com'
}

@test "--header with a newline should not send the header preceding it" {
	run xrdcp -H $'X-Test-Header: a\r\nHost: evil.example.com' $HOST//examplefile -
	refuted 'X-Test-Header: a'
}

#
# headers the request builds for itself and cannot have overridden
#

@test "--header naming Host fails" {
	run ! xrdcp -H 'Host: evil.example.com' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header naming Transfer-Encoding fails" {
	run ! xrdcp -H 'Transfer-Encoding: chunked' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header naming Content-Length fails" {
	run ! xrdcp -H 'Content-Length: 0' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header naming Connection fails" {
	run ! xrdcp -H 'Connection: close' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "a reserved header should reject the whole command line" {
	run ! xrdcp -H 'X-Test-Header: e2e-value' -H 'Host: evil.example.com' \
		$HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "a rejected command line should not send the reserved header" {
	run xrdcp -H 'X-Test-Header: e2e-value' -H 'Host: evil.example.com' \
		$HOST//examplefile -
	refuted 'Host: evil.example.com'
}

@test "a rejected command line should not send the accepted header either" {
	run xrdcp -H 'X-Test-Header: e2e-value' -H 'Host: evil.example.com' \
		$HOST//examplefile -
	refuted 'X-Test-Header: e2e-value'
}

#
# the same headers aimed at the far server of a third party copy, which the
# TransferHeader prefix must not let through
#

@test "--header naming TransferHeaderHost fails" {
	run ! xrdcp -H 'TransferHeaderHost: evil.example.com' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header naming TransferHeaderTransfer-Encoding fails" {
	run ! xrdcp -H 'TransferHeaderTransfer-Encoding: chunked' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header naming TransferHeaderContent-Length fails" {
	run ! xrdcp -H 'TransferHeaderContent-Length: 0' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "--header naming TransferHeaderConnection fails" {
	run ! xrdcp -H 'TransferHeaderConnection: close' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "a reserved TransferHeader should reject the whole command line" {
	run ! xrdcp -H 'X-Test-Header: e2e-value' \
		-H 'TransferHeaderHost: evil.example.com' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

@test "a rejected command line should not send the reserved TransferHeader" {
	run xrdcp -H 'X-Test-Header: e2e-value' \
		-H 'TransferHeaderHost: evil.example.com' $HOST//examplefile -
	refuted 'TransferHeaderHost: evil.example.com'
}

@test "a command line rejected for a TransferHeader should not send the accepted header either" {
	run xrdcp -H 'X-Test-Header: e2e-value' \
		-H 'TransferHeaderHost: evil.example.com' $HOST//examplefile -
	refuted 'X-Test-Header: e2e-value'
}
