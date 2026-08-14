#!/usr/bin/env bash

# what the client log is allowed to say about a header, one message per fault

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

HOST=http://localhost:1097

setup() {
	# workdir as test tmp dir (all files are removed after execution)
	cd $BATS_TEST_TMPDIR
}

#
# a header name reaches the log, a header value never does
#

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
		run xrdcp -H 'X-Test-Header: e2e-value' $HOST//examplefile -
	run -0 grep -F -- 'X-Test-Header' client.log
}

# The name of a refused header reaches the log, but its value never does.
@test "a refused header should not write its value to the client log" {
	XRD_LOGLEVEL=Error XRD_LOGFILE=client.log \
		run ! xrdcp -H 'Host: evil.example.com' $HOST//examplefile -
	run ! grep -F -- 'evil.example.com' client.log
}

#
# the message the client log holds for each fault
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
