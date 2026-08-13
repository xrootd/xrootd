#!/usr/bin/env bash

# $XRD_HTTPHEADERS is a second entry path that never passes through the xrdcp
# command line, so the plug-in has to apply the same rules by itself

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

	# route http:// through the XrdClHttp plug-in
	XRD_PLUGINCONFDIR=$BATS_TEST_TMPDIR/client.plugins.d
	mkdir -p $XRD_PLUGINCONFDIR
	cp $BATS_TEST_DIRNAME/http.conf $XRD_PLUGINCONFDIR/

	echo 'example file!' | xrdcp - $ROOT//examplefile
}

teardown() {
    kill_pid_files
}

@test "XRD_HTTPHEADERS should succeed" {
	XRD_HTTPHEADERS='X-Test-Header: env-value' run -0 xrdcp $HOST//examplefile -
}

@test "XRD_HTTPHEADERS should return the file contents" {
	XRD_HTTPHEADERS='X-Test-Header: env-value' run xrdcp $HOST//examplefile -
	assert_output 'example file!'
}

@test "XRD_HTTPHEADERS should be honoured" {
	XRD_HTTPHEADERS='X-Test-Header: env-value' run xrdcp $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Test-Header: env-value' header.log
}

@test "XRD_HTTPHEADERS naming a reserved header fails" {
	XRD_HTTPHEADERS='Host: evil.example.com' run ! xrdcp $HOST//examplefile -
}

@test "XRD_HTTPHEADERS naming a reserved header should not send it" {
	XRD_HTTPHEADERS='Host: evil.example.com' run xrdcp $HOST//examplefile -
	run ! grep -F -- 'got hdr line: Host: evil.example.com' header.log
}

@test "XRD_HTTPHEADERS that cannot be parsed fails" {
	XRD_HTTPHEADERS='nocolon' run ! xrdcp $HOST//examplefile -
}
