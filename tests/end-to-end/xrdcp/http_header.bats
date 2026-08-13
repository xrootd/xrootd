#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

HOST=http://localhost:1097
ROOT=root://localhost:1097

setup() {
	# workdir as test tmp dir (all files are removed after execution)
	cd $BATS_TEST_TMPDIR

	launch_xrootd http_header.cfg http_header

	sleep 0.5

	# route http:// through the XrdClHttp plug-in
	XRD_PLUGINCONFDIR=$BATS_TEST_TMPDIR/client.plugins.d
	mkdir -p $XRD_PLUGINCONFDIR
	cp $BATS_TEST_DIRNAME/http.conf $XRD_PLUGINCONFDIR/

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
	run -0 grep -F -- 'got hdr line: X-Test-Header: e2e-value' http_header.log
}

@test "long form --header should succeed" {
	run -0 xrdcp --header 'X-Test-Header: e2e-value' $HOST//examplefile -
}

@test "long form --header should send the header" {
	run xrdcp --header 'X-Test-Header: e2e-value' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Test-Header: e2e-value' http_header.log
}

@test "repeating --header should send the first header" {
	run xrdcp -H 'X-First: one' -H 'X-Second: two' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-First: one' http_header.log
}

@test "repeating --header should send the second header" {
	run xrdcp -H 'X-First: one' -H 'X-Second: two' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Second: two' http_header.log
}

@test "a header value containing spaces and colons should be sent verbatim" {
	run xrdcp -H 'X-Test-Header: a:b c:d' $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Test-Header: a:b c:d' http_header.log
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
	run -0 grep -F -- 'got hdr line: X-Test-Header: e2e-value' http_header.log
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
	run ! grep -F -- 'got hdr line: X-Test-Header' http_header.log
}

#
# --header only applies to http and https endpoints
#

@test "--header with a root source and destination fails" {
	run ! xrdcp -H 'X-Test-Header: e2e-value' $ROOT//examplefile -
}

@test "--header with a local source and destination fails" {
	echo 'local file!' > localfile
	run ! xrdcp -H 'X-Test-Header: e2e-value' localfile localcopy
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

@test "-H with no argument fails" {
	run ! xrdcp -H
}

@test "--header with no argument fails" {
	run ! xrdcp --header
}

#
# arguments that try to forge a request
#

@test "--header with a carriage return in the value fails" {
	run ! xrdcp -H $'X-Test-Header: a\rEvil: b' $HOST//examplefile -
}

@test "--header with a carriage return should not send the forged header" {
	run xrdcp -H $'X-Test-Header: a\rEvil: b' $HOST//examplefile -
	run ! grep -F -- 'got hdr line: Evil: b' http_header.log
}

# A newline in the argument separates headers rather than forging one, and each
# resulting header is validated in its own right.
@test "--header with a newline cannot smuggle a reserved header" {
	run ! xrdcp -H $'X-Test-Header: a\r\nHost: evil.example.com' $HOST//examplefile -
}

@test "--header with a newline should not send the smuggled header" {
	run xrdcp -H $'X-Test-Header: a\r\nHost: evil.example.com' $HOST//examplefile -
	run ! grep -F -- 'got hdr line: Host: evil.example.com' http_header.log
}

@test "--header with a newline should not send the header preceding it" {
	run xrdcp -H $'X-Test-Header: a\r\nHost: evil.example.com' $HOST//examplefile -
	run ! grep -F -- 'got hdr line: X-Test-Header: a' http_header.log
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

@test "--header naming Authorization fails" {
	run ! xrdcp -H 'Authorization: Bearer token' $HOST//examplefile -
}

@test "--header naming TransferHeaderAuthorization fails" {
	run ! xrdcp -H 'TransferHeaderAuthorization: Bearer token' $HOST//examplefile -
}

@test "a reserved header should reject the whole command line" {
	run ! xrdcp -H 'X-Test-Header: e2e-value' -H 'Host: evil.example.com' \
		$HOST//examplefile -
}

@test "a rejected command line should not send the reserved header" {
	run xrdcp -H 'X-Test-Header: e2e-value' -H 'Host: evil.example.com' \
		$HOST//examplefile -
	run ! grep -F -- 'got hdr line: Host: evil.example.com' http_header.log
}

@test "a rejected command line should not send the accepted header either" {
	run xrdcp -H 'X-Test-Header: e2e-value' -H 'Host: evil.example.com' \
		$HOST//examplefile -
	run ! grep -F -- 'got hdr line: X-Test-Header: e2e-value' http_header.log
}

#
# $XRD_HTTPHEADERS is a second entry path that never passes through the xrdcp
# command line, so the plug-in has to apply the same rules by itself
#

@test "XRD_HTTPHEADERS should succeed" {
	XRD_HTTPHEADERS='X-Test-Header: env-value' run -0 xrdcp $HOST//examplefile -
}

@test "XRD_HTTPHEADERS should return the file contents" {
	XRD_HTTPHEADERS='X-Test-Header: env-value' run xrdcp $HOST//examplefile -
	assert_output 'example file!'
}

@test "XRD_HTTPHEADERS should be honoured" {
	XRD_HTTPHEADERS='X-Test-Header: env-value' run xrdcp $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Test-Header: env-value' http_header.log
}

@test "XRD_HTTPHEADERS naming a reserved header fails" {
	XRD_HTTPHEADERS='Host: evil.example.com' run ! xrdcp $HOST//examplefile -
}

@test "XRD_HTTPHEADERS naming a reserved header should not send it" {
	XRD_HTTPHEADERS='Host: evil.example.com' run xrdcp $HOST//examplefile -
	run ! grep -F -- 'got hdr line: Host: evil.example.com' http_header.log
}

@test "XRD_HTTPHEADERS that cannot be parsed fails" {
	XRD_HTTPHEADERS='nocolon' run ! xrdcp $HOST//examplefile -
}

#
# what a failed or a logged transfer is allowed to say about the headers
#

# A header that cannot be honoured must fail the transfer rather than quietly
# sending a request without it, which for a credential would look like a denial.
@test "an unusable header should not be silently dropped" {
	run xrdcp -H 'nocolon' $HOST//examplefile -
	assert_output --partial 'Invalid header requested'
}

# A reserved header is refused before any request is built, so neither its name
# nor its value can reach the log.
@test "a rejected credential should not be written to the client log" {
	XRD_LOGLEVEL=Dump XRD_LOGFILE=client.log \
		run ! xrdcp -H 'Authorization: Bearer SUPERSECRETTOKEN' $HOST//examplefile -
	run ! grep -F -- 'SUPERSECRETTOKEN' client.log
}

# The name is reported so that a transfer can be told apart from one that sent
# no header at all.
@test "a header name should be written to the client log" {
	XRD_LOGLEVEL=Dump XRD_LOGFILE=client.log \
		run -0 xrdcp -H 'X-Test-Header: e2e-value' $HOST//examplefile -
	run -0 grep -F -- 'X-Test-Header' client.log
}

#
# what the client log says about a header it refused, one message per fault
#

@test "a header without a separator should say the colon is missing" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H 'nocolon' $HOST//examplefile -
	run -0 grep -F -- 'Requested header nocolon holds no colon' client.log
}

@test "a header without a name should say the name is missing" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H ': novalue' $HOST//examplefile -
	run -0 grep -F -- 'Requested header holds no name before its colon' client.log
}

@test "a header with a space in the name should say the name is not a token" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H 'X Test: value' $HOST//examplefile -
	run -0 grep -F -- 'Requested header X Test holds a name that is not an HTTP token' client.log
}

@test "a header with a parenthesis in the name should say the name is not a token" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H 'X(Test): value' $HOST//examplefile -
	run -0 grep -F -- 'Requested header X(Test) holds a name that is not an HTTP token' client.log
}

@test "a reserved header should say the client must not set it" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H 'Host: evil.example.com' $HOST//examplefile -
	run -0 grep -F -- 'Requested header Host must not be set by the client' client.log
}

@test "a header without a value should say the value is missing" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H 'X-Test-Header:' $HOST//examplefile -
	run -0 grep -F -- 'Requested header X-Test-Header holds no value' client.log
}

@test "a header with a carriage return should say the value holds one" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H $'X-Test-Header: a\rEvil: b' $HOST//examplefile -
	run -0 grep -F -- 'Requested header X-Test-Header holds a carriage return in its value' client.log
}

# Every header is examined, so a command line with several mistakes reports each
# of them instead of only the first.
@test "two unusable headers should report the first one" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H 'nocolon' -H 'X-Test-Header:' $HOST//examplefile -
	run -0 grep -F -- 'Requested header nocolon holds no colon' client.log
}

@test "two unusable headers should report the second one" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H 'nocolon' -H 'X-Test-Header:' $HOST//examplefile -
	run -0 grep -F -- 'Requested header X-Test-Header holds no value' client.log
}

# The name of a refused header reaches the log, but its value never does.
@test "a refused header should not write its value to the client log" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H 'Host: evil.example.com' $HOST//examplefile -
	run ! grep -F -- 'evil.example.com' client.log
}

#
# options -H took over
#

@test "--license is still available after -H was reassigned" {
	run xrdcp --license
	assert_output --partial 'Copyright'
}
