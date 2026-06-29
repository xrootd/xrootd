#!/usr/bin/env bash

# End-to-end test for the xrdmoncollect terminal operation reports (WLCG
# operation_state / error_message / error_category). A real xrootd server is
# pointed at a local xrdmoncollect instance; we drive a successful transfer and
# a failed open, then assert the collector emits the matching documents.

COLLECTOR_PORT=8096
COLLECTOR_OUT="${PWD}/${NAME}/collected.ndjson"
COLLECTOR_PID="${PWD}/${NAME}/collector.pid"

function setup_moncollect() {
	require_commands xrdmoncollect xrdcp xrdfs xrdreadv-eof

	# Start the collector before the server so the f-stream destination has a
	# listener. The PID file lands in ${NAME}/ so the harness teardown kills it.
	# Redirect all of its fds (and detach) so it does not inherit and hold open
	# the test runner's stdout pipe, which would block ctest.
	: > "${COLLECTOR_OUT}"
	xrdmoncollect -p "${COLLECTOR_PORT}" -o "${COLLECTOR_OUT}" \
	              --flush-secs 1 --flush-count 1 \
	              > "${PWD}/${NAME}/collector.log" 2>&1 < /dev/null &
	echo $! > "${COLLECTOR_PID}"
	disown 2>/dev/null || true

	# Give it a moment to bind the socket.
	sleep 1
}

# Re-run an action each second (monitoring is UDP, so a single burst may be
# lost or race the server's monitor startup) until the collector output matches
# the given grep -E pattern, up to ~60s.
function drive_until() {
	local pattern=$1 desc=$2 action=$3 i
	for i in $(seq 1 60); do
		eval "${action}" >/dev/null 2>&1 || true
		if grep -Eq "${pattern}" "${COLLECTOR_OUT}" 2>/dev/null; then
			echo "found: ${desc}"
			return 0
		fi
		sleep 1
	done
	echo 1>&2 "=== collector output (${COLLECTOR_OUT}) ==="
	cat 1>&2 "${COLLECTOR_OUT}" || true
	error "timed out waiting for: ${desc}"
}

function test_moncollect() {
	echo "server: XRootD $(assert xrdfs "${HOST}" query config version 2>&1)"

	TMPDIR=$(mktemp -d "${PWD}/${NAME}/test-XXXXXX")
	assert xrdfs "${HOST}" mkdir -p "${TMPDIR}"
	assert openssl rand -base64 -out "${TMPDIR}/ok.ref" $((1024 * 64))

	# 1. A successful transfer: upload then download a file. The close produces
	#    a transfer document with operation_state "Successful". Re-driven each
	#    second until the document is observed (tolerates UDP loss).
	drive_until '"operation_state":"Successful"' "successful transfer document" \
		"xrdcp -f '${TMPDIR}/ok.ref' '${HOST}/${TMPDIR}/ok.ref' \
		 && xrdcp -f '${HOST}/${TMPDIR}/ok.ref' '${TMPDIR}/ok.dat'"

	# 2. A failed open: reading a nonexistent file fails before any close, so
	#    the server emits a terminal isError record -> operation_state "Failed".
	drive_until '"operation_state":"Failed"' "failed-open document" \
		"xrdcp '${HOST}/${TMPDIR}/does-not-exist.root' '${TMPDIR}/x.dat'"

	# The failed document must name the missing file and carry an error message.
	assert grep -Eq '"lfn":"[^"]*does-not-exist.root"' "${COLLECTOR_OUT}"
	assert grep -Eq '"error_message":"[^"]+"' "${COLLECTOR_OUT}"
	assert grep -Eq '"error_category":"open"' "${COLLECTOR_OUT}"

	# 3. A mid-transfer read error: a vector read past EOF fails on the server,
	#    which records the terminal error on the file so its close reports
	#    operation_state "Failed" with error_category "read". xrdreadv-eof opens
	#    the existing file and issues the failing readv, then closes.
	drive_until '"error_category":"read"' "failed-readv close document" \
		"xrdreadv-eof '${HOST}/${TMPDIR}/ok.ref'"

	echo "collector documents:"
	cat "${COLLECTOR_OUT}"
}
