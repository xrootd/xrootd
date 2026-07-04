#!/usr/bin/env bash

# End-to-end test for the xrdmoncollect TCP shovel chain. The server's monitor
# f-stream goes over UDP to a local xrdmoncollect in shoveler mode, which
# relays the datagrams XSHV-framed over TCP (with a shared-secret hello) to a
# central TCP-only collector (--tcp-port, no -p), which must emit the usual
# transfer documents attributed to the ORIGINAL sender, not the shoveler. A
# collector outage mid-test exercises the disk spool and its replay.

SHOVEL_UDP_PORT=8192
CENTRAL_TCP_PORT=8193

CENTRAL_OUT="${PWD}/${NAME}/collected.ndjson"
CENTRAL_PID="${PWD}/${NAME}/central.pid"
SHOVELER_PID="${PWD}/${NAME}/shoveler.pid"
SPOOL_DIR="${PWD}/${NAME}/spool"
TOKEN_FILE="${PWD}/${NAME}/shovel.token"

# Start (or restart) the central collector: TCP-only (no UDP port) and
# token-protected, writing NDJSON documents to ${CENTRAL_OUT}. The PID file
# lands in ${NAME}/ so the harness teardown kills it; fds are redirected (and
# the job detached) so it does not hold open the test runner's stdout pipe.
function start_central() {
	xrdmoncollect --tcp-port "${CENTRAL_TCP_PORT}" --tcp-token "@${TOKEN_FILE}" \
	              -o "${CENTRAL_OUT}" --flush-secs 1 --flush-count 1 \
	              >> "${PWD}/${NAME}/central.log" 2>&1 < /dev/null &
	echo $! > "${CENTRAL_PID}"
	disown 2>/dev/null || true
}

function setup_shovel() {
	require_commands xrdmoncollect xrdcp xrdfs

	printf 'shovel-e2e-secret' > "${TOKEN_FILE}"
	: > "${CENTRAL_OUT}"

	# Bring up the chain back to front (collector, then shoveler) before the
	# server starts, so the first monitor datagram already has a full path.
	start_central

	xrdmoncollect -p "${SHOVEL_UDP_PORT}" \
	              --shovel "127.0.0.1:${CENTRAL_TCP_PORT}" \
	              --shovel-token "@${TOKEN_FILE}" \
	              --cache-dir "${SPOOL_DIR}" \
	              --flush-secs 1 --flush-count 1 \
	              > "${PWD}/${NAME}/shoveler.log" 2>&1 < /dev/null &
	echo $! > "${SHOVELER_PID}"
	disown 2>/dev/null || true

	# Give both a moment to bind their sockets.
	sleep 1
}

# Re-run an action each second (the UDP leg to the shoveler is still UDP, so a
# single burst may be lost or race the server's monitor startup) until the
# central collector's output matches the given grep -E pattern, up to ~60s.
function drive_until() {
	local pattern=$1 desc=$2 action=$3 i
	for i in $(seq 1 60); do
		eval "${action}" >/dev/null 2>&1 || true
		if grep -Eq "${pattern}" "${CENTRAL_OUT}" 2>/dev/null; then
			echo "found: ${desc}"
			return 0
		fi
		sleep 1
	done
	echo 1>&2 "=== central collector output (${CENTRAL_OUT}) ==="
	cat 1>&2 "${CENTRAL_OUT}" || true
	echo 1>&2 "=== shoveler log ==="
	cat 1>&2 "${PWD}/${NAME}/shoveler.log" || true
	error "timed out waiting for: ${desc}"
}

function test_shovel() {
	echo "server: XRootD $(assert xrdfs "${HOST}" query config version 2>&1)"

	TMPDIR=$(mktemp -d "${PWD}/${NAME}/test-XXXXXX")
	assert xrdfs "${HOST}" mkdir -p "${TMPDIR}"
	assert openssl rand -base64 -out "${TMPDIR}/ok.ref" $((1024 * 64))

	# 1. A successful transfer through the whole chain: server -> UDP ->
	#    shoveler -> TCP -> central collector -> document.
	drive_until '"xrootd.operation_state":"Successful"' \
		"successful transfer document (via shovel chain)" \
		"xrdcp -f '${TMPDIR}/ok.ref' '${HOST}/${TMPDIR}/ok.ref' \
		 && xrdcp -f '${HOST}/${TMPDIR}/ok.ref' '${TMPDIR}/ok.dat'"

	# Attribution: the frames carry the datagram's original source address, so
	# the central collector must resolve the server identity exactly as it
	# would for direct UDP reception — a real name, never a loopback literal
	# or the "unknown" placeholder, and the configured sitename verbatim.
	assert grep -Eq '"server.address":"[^"]+"' "${CENTRAL_OUT}"
	assert_failure grep -Eq '"server\.address":"(unknown|localhost[^"]*|127\.[0-9.]+|::1|::ffff:127\.[0-9.]+)"' "${CENTRAL_OUT}"
	assert grep -Eq '"xrootd.server.site":"shovel"' "${CENTRAL_OUT}"

	# 2. Collector outage: kill the central collector and keep transferring a
	#    uniquely named file. The first buffer sent after the peer dies can
	#    land in the dead connection's kernel buffer (plain TCP has no
	#    application-level ack), but once the RST arrives every subsequent
	#    buffer must spool to disk under ${SPOOL_DIR}/shovel.
	kill -s TERM "$(cat "${CENTRAL_PID}")" 2>/dev/null || true
	for _ in $(seq 1 30); do
		ps -o pid= "$(cat "${CENTRAL_PID}")" >/dev/null 2>&1 || break
		sleep 1
	done

	UNIQUE="replay-$$"
	for _ in $(seq 1 30); do
		xrdcp -f "${TMPDIR}/ok.ref" "${HOST}/${TMPDIR}/${UNIQUE}.ref" >/dev/null 2>&1 || true
		compgen -G "${SPOOL_DIR}/shovel/*.frames" >/dev/null && break
		sleep 1
	done
	assert compgen -G "${SPOOL_DIR}/shovel/*.frames"
	echo "found: shovel spool files during collector outage"

	# With the spool now active, further transfers are guaranteed to queue
	# behind the on-disk backlog (order is preserved), so the close records of
	# these two are on disk before the collector returns.
	for _ in 1 2; do
		assert xrdcp -f "${TMPDIR}/ok.ref" "${HOST}/${TMPDIR}/${UNIQUE}.ref"
		sleep 3   # fstat flush (2s) + shoveler batch flush (1s)
	done

	# 3. Restart the central collector: the shoveler must reconnect and replay
	#    the spooled frames oldest-first, and the unique file's transfer
	#    document must appear WITHOUT any new transfer being driven.
	start_central
	drive_until "\"file.path\":\"[^\"]*${UNIQUE}\\.ref\"" \
		"replayed transfer document after collector restart" \
		"true"

	echo "central collector documents:"
	cat "${CENTRAL_OUT}"
}
