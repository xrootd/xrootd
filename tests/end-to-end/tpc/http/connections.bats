#!/usr/bin/env bash

load ../../helper/common.bash

export XRD_LOGLEVEL=Debug
export XRD_PLUGINCONFDIR="$BATS_TEST_DIRNAME"
export no_proxy=127.0.0.1
export NO_PROXY=127.0.0.1

readonly XROOTD_SRC="127.0.0.1:8568"
readonly XROOTD_DST="127.0.0.1:8569"
readonly XROOTD_RDR="127.0.0.1:8570"

setup() {
	cd $BATS_TEST_TMPDIR

	PORT=${XROOTD_SRC##*:} launch_xrootd connections-src.cfg xrootd_src
	PORT=${XROOTD_DST##*:} launch_xrootd bigfiles.cfg xrootd_dst
	PORT=${XROOTD_RDR##*:} launch_xrootd connections-rdr.cfg xrootd_rdr

	head -c 1 /dev/zero | xrdcp - http://${XROOTD_SRC}//file_src
	xrdfs root://${XROOTD_SRC}/ truncate /file_src 1073741824
}

teardown() {
	kill_pid_files
}

# Print the client sockets to the port which are established, or closed by the server only
connections() {
	local port=$1

	if [[ -r /proc/net/tcp ]]; then
		# the remote port is in hex; state 01 is ESTABLISHED, 08 is CLOSE_WAIT
		cat /proc/net/tcp /proc/net/tcp6 2>/dev/null | awk -v port=":$(printf '%04X' "$port")" \
			'substr($3, length($3) - 4) == port && ($4 == "01" || $4 == "08")'
	else
		netstat -an -p tcp | awk -v port=".$port" \
			'substr($5, length($5) - length(port) + 1) == port && ($6 == "ESTABLISHED" || $6 == "CLOSE_WAIT")'
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
	curl -s -X COPY -H "Source: $source" "http://${XROOTD_DST}/file_dst" >/dev/null 2>&1 3>&- &
	echo $! > "$BATS_TEST_TMPDIR/copy.pid"

	for _ in {1..200}; do
		grep -qsF 'got hdr line: GET /file_src' xrootd_src.log && return 0
		sleep 0.05
	done

	echo "the pull copy did not send its GET to the source" >&2
	return 1
}

@test "pull via a redirector holds no idle connection" {
	start_pull http://${XROOTD_RDR}/file_src
	assert_connections ${XROOTD_RDR##*:} 0
	assert_connections ${XROOTD_SRC##*:} 1
}

@test "pull from the source holds a single connection" {
	start_pull http://${XROOTD_SRC}/file_src
	assert_connections ${XROOTD_SRC##*:} 1
}
