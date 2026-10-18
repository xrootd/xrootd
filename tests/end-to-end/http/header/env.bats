#!/usr/bin/env bash

# $XRD_HTTPHEADERS is a second entry path that never passes through the xrdcp
# command line, so the plug-in has to apply the same rules by itself

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

@test "XRD_HTTPHEADERS should succeed" {
	XRD_HTTPHEADERS='X-Test-Header: env-value' run -0 xrdcp $HOST//examplefile -
}

@test "XRD_HTTPHEADERS should return the file contents" {
	XRD_HTTPHEADERS='X-Test-Header: env-value' run xrdcp $HOST//examplefile -
	assert_output 'example file!'
}

@test "XRD_HTTPHEADERS should be honoured" {
	XRD_HTTPHEADERS='X-Test-Header: env-value' run xrdcp $HOST//examplefile -
	run -0 grep -F -- 'got hdr line: X-Test-Header: env-value' <(server_log server)
}

@test "XRD_HTTPHEADERS naming a reserved header fails" {
	XRD_HTTPHEADERS='Host: evil.example.com' run ! xrdcp $HOST//examplefile -
}

@test "XRD_HTTPHEADERS naming a reserved header should not send it" {
	XRD_HTTPHEADERS='Host: evil.example.com' run xrdcp $HOST//examplefile -
	run ! grep -F -- 'got hdr line: Host: evil.example.com' <(server_log server)
}

@test "XRD_HTTPHEADERS that cannot be parsed fails" {
	XRD_HTTPHEADERS='nocolon' run ! xrdcp $HOST//examplefile -
}
