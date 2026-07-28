#!/usr/bin/env bash

# the endpoints --header applies to: one side of the transfer must speak http
# or https, in either direction

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

export XRD_PLUGINCONFDIR="$BATS_TEST_DIRNAME"

HOST=http://localhost:9879
HOSTS=https://localhost:9879
ROOT=root://localhost:9879
S3=s3://localhost:9879
DAV=dav://localhost:9879
DAVS=davs://localhost:9879

# The endpoints are examined before any connection is made, so these tests need
# no server. This is the message the command line check writes.
REFUSAL="'--header' requires an http or https source or destination."

setup() {
	# workdir as test tmp dir (all files are removed after execution)
	cd $BATS_TEST_TMPDIR

	echo 'local file!' > localfile
}

#
# a transfer that reaches no http or https endpoint
#

@test "--header on a root download fails" {
	run ! xrdcp -H 'X-Test-Header: e2e-value' $ROOT//examplefile -
}

@test "--header on a root download should say an http endpoint is required" {
	run xrdcp -H 'X-Test-Header: e2e-value' $ROOT//examplefile -
	assert_output --partial "$REFUSAL"
}

@test "--header on a root upload fails" {
	run ! xrdcp -H 'X-Test-Header: e2e-value' localfile $ROOT//uploadedfile
}

@test "--header on a root upload should say an http endpoint is required" {
	run xrdcp -H 'X-Test-Header: e2e-value' localfile $ROOT//uploadedfile
	assert_output --partial "$REFUSAL"
}

@test "--header with a root source and a root destination fails" {
	run ! xrdcp -H 'X-Test-Header: e2e-value' $ROOT//examplefile $ROOT//copiedfile
}

@test "--header with a root source and a root destination should say an http endpoint is required" {
	run xrdcp -H 'X-Test-Header: e2e-value' $ROOT//examplefile $ROOT//copiedfile
	assert_output --partial "$REFUSAL"
}

@test "--header with a local source and destination fails" {
	run ! xrdcp -H 'X-Test-Header: e2e-value' localfile localcopy
}

@test "--header with a local source and destination should say an http endpoint is required" {
	run xrdcp -H 'X-Test-Header: e2e-value' localfile localcopy
	assert_output --partial "$REFUSAL"
}

#
# a transfer that reaches an http or https endpoint. No server runs here, so
# each transfer fails on the connection instead. What the tests assert is that
# the endpoints themselves were accepted.
#

@test "--header on an http download should not be refused for its endpoints" {
	run xrdcp -H 'X-Test-Header: e2e-value' $HOST//examplefile -
	refute_output --partial "$REFUSAL"
}

@test "--header on an http upload should not be refused for its endpoints" {
	run xrdcp -H 'X-Test-Header: e2e-value' localfile $HOST//uploadedfile
	refute_output --partial "$REFUSAL"
}

@test "--header on an https download should not be refused for its endpoints" {
	run xrdcp -H 'X-Test-Header: e2e-value' $HOSTS//examplefile -
	refute_output --partial "$REFUSAL"
}

@test "--header on an https upload should not be refused for its endpoints" {
	run xrdcp -H 'X-Test-Header: e2e-value' localfile $HOSTS//uploadedfile
	refute_output --partial "$REFUSAL"
}

#
# S3 speaks http under the hood, so it must be accepted like http and https.
# No server runs here, so each transfer fails on the connection instead. What
# the tests assert is that the endpoints themselves were accepted.
#

@test "--header on an s3 download should not be refused for its endpoints" {
	run xrdcp -H 'X-Test-Header: e2e-value' $S3//examplefile -
	refute_output --partial "$REFUSAL"
}

@test "--header on an s3 upload should not be refused for its endpoints" {
	run xrdcp -H 'X-Test-Header: e2e-value' localfile $S3//uploadedfile
	refute_output --partial "$REFUSAL"
}

#
# dav and davs are the WebDAV aliases for http and https, so they must be
# accepted the same way. No server runs here, so each transfer fails on the
# connection instead. What the tests assert is that the endpoints themselves
# were accepted.
#

@test "--header on a dav download should not be refused for its endpoints" {
	run xrdcp -H 'X-Test-Header: e2e-value' $DAV//examplefile -
	refute_output --partial "$REFUSAL"
}

@test "--header on a dav upload should not be refused for its endpoints" {
	run xrdcp -H 'X-Test-Header: e2e-value' localfile $DAV//uploadedfile
	refute_output --partial "$REFUSAL"
}

@test "--header on a davs download should not be refused for its endpoints" {
	run xrdcp -H 'X-Test-Header: e2e-value' $DAVS//examplefile -
	refute_output --partial "$REFUSAL"
}

@test "--header on a davs upload should not be refused for its endpoints" {
	run xrdcp -H 'X-Test-Header: e2e-value' localfile $DAVS//uploadedfile
	refute_output --partial "$REFUSAL"
}

#
# a third party copy, which the command line examines the same way. Both
# endpoints are remote, so a local file takes no part here. The copy itself is
# refused later, for a reason of its own, so each accepted case asserts the
# endpoint check alone.
#

@test "--header on a root third party copy should say an http endpoint is required" {
	run xrdcp --tpc only -H 'X-Test-Header: e2e-value' \
		$ROOT//examplefile $ROOT//copiedfile
	assert_output --partial "$REFUSAL"
}

@test "--header on an http third party copy should not be refused for its endpoints" {
	run xrdcp --tpc only -H 'X-Test-Header: e2e-value' \
		$HOST//examplefile $HOST//copiedfile
	refute_output --partial "$REFUSAL"
}

@test "--header on an https third party copy should not be refused for its endpoints" {
	run xrdcp --tpc only -H 'X-Test-Header: e2e-value' \
		$HOSTS//examplefile $HOSTS//copiedfile
	refute_output --partial "$REFUSAL"
}

@test "--header on a third party copy from http to https should not be refused for its endpoints" {
	run xrdcp --tpc only -H 'X-Test-Header: e2e-value' \
		$HOST//examplefile $HOSTS//copiedfile
	refute_output --partial "$REFUSAL"
}

@test "--header on a third party copy from https to http should not be refused for its endpoints" {
	run xrdcp --tpc only -H 'X-Test-Header: e2e-value' \
		$HOSTS//examplefile $HOST//copiedfile
	refute_output --partial "$REFUSAL"
}

@test "--header on an s3 third party copy should not be refused for its endpoints" {
	run xrdcp --tpc only -H 'X-Test-Header: e2e-value' \
		$S3//examplefile $S3//copiedfile
	refute_output --partial "$REFUSAL"
}

@test "--header on a dav third party copy should not be refused for its endpoints" {
	run xrdcp --tpc only -H 'X-Test-Header: e2e-value' \
		$DAV//examplefile $DAV//copiedfile
	refute_output --partial "$REFUSAL"
}

@test "--header on a davs third party copy should not be refused for its endpoints" {
	run xrdcp --tpc only -H 'X-Test-Header: e2e-value' \
		$DAVS//examplefile $DAVS//copiedfile
	refute_output --partial "$REFUSAL"
}
