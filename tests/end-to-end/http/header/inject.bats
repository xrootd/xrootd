#!/usr/bin/env bash

# a header given on the command line reaches the server

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"

setup_file() {
	require_plugin libXrdHttp
	start_server server server http
	echo 'example file!' > "$SERVER_DATA/examplefile"
}

teardown_file() {
	stop_servers
}

setup() {
	cd "$BATS_TEST_TMPDIR"
	HOST=$SERVER_HTTP
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
	run -0 grep -F -- 'got hdr line: X-Test-Header: e2e-value' <(server_log server)
}

@test "long form --header should succeed" {
	run -0 xrdcp --header 'X-Test-Header: e2e-value' $HOST//examplefile -
}

@test "long form --header should send the header" {
	run xrdcp --header 'X-Test-Header: e2e-value' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Test-Header: e2e-value' <(server_log server)
}

@test "repeating --header should send the first header" {
	run xrdcp --header 'X-First: one' --header 'X-Second: two' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-First: one' <(server_log server)
}

@test "repeating --header should send the second header" {
	run xrdcp --header 'X-First: one' --header 'X-Second: two' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Second: two' <(server_log server)
}

@test "a header value containing spaces and colons should be sent verbatim" {
	run xrdcp -H 'X-Test-Header: a:b c:d' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Test-Header: a:b c:d' <(server_log server)
}

# A user who holds a token has to be able to present it, so Authorization is the
# caller's to set. The server log holds the name but redacts the credential, so
# the tests below assert the name alone.

@test "download with an injected Authorization header should succeed" {
	run -0 xrdcp -H 'Authorization: Bearer e2e-token' $HOST//examplefile -
}

@test "download with an injected Authorization header should send the header" {
	run xrdcp -H 'Authorization: Bearer e2e-token' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: Authorization:' <(server_log server)
}

@test "download with an injected TransferHeaderAuthorization header should succeed" {
	run -0 xrdcp -H 'TransferHeaderAuthorization: Bearer e2e-token' $HOST//examplefile -
}

@test "download with an injected TransferHeaderAuthorization header should send the header" {
	run xrdcp -H 'TransferHeaderAuthorization: Bearer e2e-token' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: TransferHeaderAuthorization:' <(server_log server)
}

#
# injecting a header on upload
#

@test "upload with an injected header should succeed" {
	echo 'uploaded!' > localfile
	run -0 xrdcp -H 'X-Test-Header: e2e-value' localfile $HOST//uploadedfile$BATS_TEST_NUMBER
}

@test "upload with an injected header should send the header" {
	echo 'uploaded!' > localfile
	run xrdcp -H 'X-Test-Header: e2e-value' localfile $HOST//uploadedfile$BATS_TEST_NUMBER
	run -0 grep -F -- 'got hdr line: X-Test-Header: e2e-value' <(server_log server)
}

@test "upload with an injected header should store the file contents" {
	echo 'uploaded!' > localfile
	run xrdcp -H 'X-Test-Header: e2e-value' localfile $HOST//uploadedfile$BATS_TEST_NUMBER
	assert_equal "$(cat "$SERVER_DATA/uploadedfile$BATS_TEST_NUMBER")" 'uploaded!'
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
	run ! grep -F -- 'got hdr line: X-Test-Header' <(server_log server)
}
