#!/usr/bin/env bash

# A PUT with 'Overwrite: F' must not replace an existing file. The HTTP TPC
# push mode forwards this header to the destination when --force is not set.

load ../../setup/server.bash

setup_file() {
	require_plugin libXrdHttp
	start_server server server http
}

teardown_file() {
	stop_servers
}

# Each test has its own file, which exists with the content 'original'
setup() {
	cd "$BATS_TEST_TMPDIR"
	HOST=$SERVER_HTTP
	FILE="file$BATS_TEST_NUMBER"

	echo 'original' > "$SERVER_DATA/$FILE"
	echo 'replacement' > replacement
}

@test "PUT without an Overwrite header replaces an existing file" {
	run -0 curl --fail --silent --show-error -T replacement $HOST/$FILE
}

@test "PUT with 'Overwrite: T' replaces an existing file" {
	curl --fail --silent --show-error -H 'Overwrite: T' -T replacement $HOST/$FILE
	run -0 curl --fail --silent --show-error $HOST/$FILE
	assert_output 'replacement'
}

@test "PUT with 'Overwrite: F' fails with 409 when the file exists" {
	run -0 curl --silent --output /dev/null --write-out '%{http_code}' -H 'Overwrite: F' -T replacement $HOST/$FILE
	assert_output '409'
}

@test "PUT with 'Overwrite: F' keeps the content of an existing file" {
	curl --silent --output /dev/null -H 'Overwrite: F' -T replacement $HOST/$FILE || true
	run -0 curl --fail --silent --show-error $HOST/$FILE
	assert_output 'original'
}

@test "PUT with 'Overwrite: F' creates a new file" {
	run -0 curl --fail --silent --show-error -H 'Overwrite: F' -T replacement $HOST/new$FILE
}
