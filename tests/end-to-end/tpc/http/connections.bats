#!/usr/bin/env bash

# The connections which the destination of a pull copy holds while the data
# flows (issue #2995). The servers of each test count the connections of a
# single copy only, thus each test starts its own servers.

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export no_proxy=localhost NO_PROXY=localhost

setup() {
	require_plugin libXrdHttpTPC
	cd "$BATS_TEST_TMPDIR"

	start_server src http.cfg http-tpc.cfg connections-src.cfg mirage-xattr.cfg trace-http.cfg
	start_server dst http.cfg http-tpc.cfg trace-http.cfg
	start_server rdr http.cfg connections-rdr.cfg trace-http.cfg

	head -c 1 /dev/zero | xrdcp - "$SRC_HTTP//file_src"
	xrdfs "$SRC_URL" truncate /file_src 1073741824
}

teardown() {
	if [[ -s "$BATS_TEST_TMPDIR/copy.pid" ]]; then
		stop_pid "$(<"$BATS_TEST_TMPDIR/copy.pid")"
	fi
	stop_servers
}

# Print the client sockets to the port which are established, or closed by the server only
connections() {
	local port=$1

	if [[ -r /proc/net/tcp ]]; then
		# the remote port is in hex; state 01 is ESTABLISHED, 08 is CLOSE_WAIT
		cat /proc/net/tcp /proc/net/tcp6 2>/dev/null | awk -v port=":$(printf '%04X' "$port")" \
			'substr($3, length($3) - 4) == port && ($4 == "01" || $4 == "08")'
	else
		lsof -nP -a -iTCP:"$port" -sTCP:ESTABLISHED,CLOSE_WAIT | awk -v port=":$port" \
			'$9 ~ /->/ && substr($9, length($9) - length(port) + 1) == port'
	fi
}

# Wait for the expected number of connections to the port, as libcurl may close them gracefully
assert_connections() {
	local port=$1 expected=$2 count

	for _ in {1..200}; do
		(( $(connections "$port" | wc -l) == expected )) && return 0
		sleep 0.05
	done

	count=$(connections "$port" | wc -l)
	echo "expected $expected connection(s) to port $port, found $((count)):" >&2
	connections "$port" >&2
	curl --version | head -n 1 >&2
	return 1
}

# Start a pull copy, the destination being the only client of the source and
# the redirector, and wait until its GET reaches the source
start_pull() {
	local source=$1

	# without fd 3, bats does not wait for the copy to end
	curl -s -X COPY -H "Source: $source" "$DST_HTTP/file_dst" >/dev/null 2>&1 3>&- &
	echo $! > "$BATS_TEST_TMPDIR/copy.pid"

	for _ in {1..200}; do
		grep -qsF 'got hdr line: GET /file_src' "$SRC_LOG" && return 0
		sleep 0.05
	done

	echo "the pull copy did not send its GET to the source" >&2
	return 1
}

@test "pull via a redirector holds no idle connection" {
	start_pull "$RDR_HTTP/file_src"
	assert_connections "$RDR_PORT" 0
	assert_connections "$SRC_PORT" 1
}

@test "pull from the source holds a single connection" {
	start_pull "$SRC_HTTP/file_src"
	assert_connections "$SRC_PORT" 1
}
