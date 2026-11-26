#!/usr/bin/env bash

# A copy whose destination is a manager. The first open of a new file stalls
# for the lookup delay while the manager asks which data server should create
# it. The TPC open must wait and try again: it used to return the stall, and
# the copy failed.
#
# A push sends Expect: 100-continue. libcurl waits one second for the answer.
# The lookup delay here is longer than that, so the push must wait for the
# redirect of the manager. curl issues the COPY. xrdcp would ask the
# destination about the file first, and that request would wait out the
# stall, so the PUT which follows would no longer race the timeout.

load ../../setup/cluster.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_REQUESTTIMEOUT=30
export no_proxy=localhost NO_PROXY=localhost

setup_file() {
	require_plugin libXrdHttpTPC
	require_commands curl
	start_server src http.cfg http-tpc.cfg
	echo 'source content' > "$SRC_DATA/file_src"
	start_manager man "" http.cfg redirector.cfg lookup.cfg
	start_data_server srv man http.cfg redirector.cfg
	wait_for_cluster man 1
}

teardown_file() {
	stop_servers
}

setup() {
	cd "$BATS_TEST_TMPDIR"
	N=$BATS_TEST_NUMBER
}

# The manager stalled the open of this test's file.
assert_manager_stalled() {
	run -0 grep -F 'delays' <(server_log man)
	assert_output --partial "/file_dst$N"
}

@test "a pull through a manager writes the file on the data server" {
	run -0 xrdcp -T only "$SRC_HTTP//file_src" "$MAN_HTTP//file_dst$N"
	assert_equal "$(<"$SRV_DATA/file_dst$N")" 'source content'
	assert_manager_stalled
}

@test "a push through a manager writes the file on the data server" {
	run -0 curl --silent --show-error --fail -X COPY \
		-H "Destination: $MAN_HTTP/file_dst$N" \
		"$SRC_HTTP/file_src"
	assert_equal "$(<"$SRV_DATA/file_dst$N")" 'source content'
	assert_manager_stalled
}
