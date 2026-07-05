#!/usr/bin/env bash

# End-to-end test for the xrdmoncollect terminal operation reports (WLCG
# operation_state / error_message / error_category). A real xrootd server is
# pointed at a local xrdmoncollect instance; we drive a successful transfer and
# a failed open, then assert the collector emits the matching documents.
#
# When the VOMS plug-in is built (MONCOLLECT_VOMS=1, set by CMake), the session
# is additionally driven over gsi with a proxy carrying a fake VOMS attribute
# certificate, and we assert the VO is surfaced on the monitoring stream
# (XrdSecEntity.vorg -> u-record "&o=" -> collector user.vo). The fake AC is
# minted with voms-proxy-fake and signed by the test host cert, which the
# server's voms library trusts via a generated vomsdir/.lsc file.

COLLECTOR_PORT=8096
COLLECTOR_OUT="${PWD}/${NAME}/collected.ndjson"
COLLECTOR_PID="${PWD}/${NAME}/collector.pid"

# Mock OTLP/HTTP receiver: when python3 is available the collector additionally
# exports OTLP logs/traces here, and the test asserts they arrive. Optional so
# the e2e still runs on hosts without python3.
OTLP_PORT=8097
OTLP_OUT="${PWD}/${NAME}/otlp.captured"
OTLP_PID="${PWD}/${NAME}/otlp.pid"
OTLP_CACHE="${PWD}/${NAME}/otlp-cache"

# Security fragment that moncollect.cfg continues into (see the cfg). Generated
# at setup time so its contents can depend on whether VOMS is built. The name has
# no underscore because XRootD's inline $var substitution stops at non-alnum.
MONCOLLECTSEC="${MONCOLLECTSEC:-${PWD}/${NAME}/security.cfg}"
export MONCOLLECTSEC

# gsi/VOMS environment, shared by the server (inherited from the setup
# invocation) and the client (run invocation). Set at script scope so it applies
# to every test.sh phase, mirroring gsi.sh.
if [ "${MONCOLLECT_VOMS}" = 1 ]; then
	TLS_DIR="${BINARY_DIR}/tests/tls"
	VOMS_DIR="${PWD}/${NAME}/vomsdir"
	export XrdSecPROTOCOL=gsi
	export X509_CERT_DIR="${TLS_DIR}"
	export X509_VOMS_DIR="${VOMS_DIR}"
	export X509_USER_PROXY="${PWD}/${NAME}/vproxy.crt"
fi

# Write the security fragment moncollect.cfg continues into. XRootD only allows a
# single level of "continue", so this fragment is the leaf: with VOMS it prepends
# the gsi + VOMS extraction directives (mirrors gsi.cfg) to a verbatim copy of
# common.cfg; without it the fragment is just common.cfg, so the test is
# unchanged on non-VOMS builds.
function write_security_fragment() {
	{
		if [ "${MONCOLLECT_VOMS}" = 1 ]; then
			cat <<-EOF
			xrootd.seclib libXrdSec.so
			sec.protparm gsi -ca:verify -certdir:${TLS_DIR}
			sec.protparm gsi -crl:require -crldir:${TLS_DIR}
			sec.protparm gsi -key:${TLS_DIR}/host.key
			sec.protparm gsi -cert:${TLS_DIR}/host.pem
			sec.protparm gsi -gridmap:${PWD}/${NAME}/gridmap
			sec.protparm gsi -gmapopt:trymap,usedn
			sec.protparm gsi -vomsat:extract -vomsfun:libXrdVoms.so -vomsfunparms:dbg
			sec.protparm gsi -md:sha512:sha256 -d:1 -trustdns:false
			sec.protocol gsi
			sec.protbind * only gsi
			ofs.authorize 1
			acc.authdb ${PWD}/${NAME}/authdb
			EOF
		fi
		cat "${SOURCE_DIR}/common.cfg"
	} >| "${MONCOLLECTSEC}"
}

# Mint the fake VOMS proxy and the trust material the server uses to verify it.
function setup_moncollect_voms() {
	require_commands voms-proxy-fake openssl

	# gsi maps the client DN to a username and authorizes it for all of /.
	cat >| "${PWD}/${NAME}/authdb" <<-EOF
	u client / a
	EOF
	cat >| "${PWD}/${NAME}/gridmap" <<-EOF
	"/CN=client" client
	EOF

	# Trust the fake AC: it is signed by the test host cert, so the .lsc lists
	# that cert's subject and issuer DNs in OpenSSL slash (compat) form.
	mkdir -p "${VOMS_DIR}/dteam"
	{
		openssl x509 -in "${TLS_DIR}/host.pem" -noout -subject -nameopt compat | sed 's/^subject=//'
		openssl x509 -in "${TLS_DIR}/host.pem" -noout -issuer  -nameopt compat | sed 's/^issuer=//'
	} >| "${VOMS_DIR}/dteam/localhost.lsc"

	# Create a proxy from the client cert carrying a fake VOMS AC for VO dteam.
	voms-proxy-fake -rfc -quiet \
		-certdir "${TLS_DIR}" \
		-cert "${TLS_DIR}/client.crt" -key "${TLS_DIR}/client.key" \
		-hostcert "${TLS_DIR}/host.pem" -hostkey "${TLS_DIR}/host.key" \
		-voms dteam -uri localhost:15000 \
		-fqan /dteam/Role=production/Capability=NULL \
		-out "${X509_USER_PROXY}"
	# gsi rejects proxies with loose permissions.
	chmod 600 "${X509_USER_PROXY}"
}

function setup_moncollect() {
	require_commands xrdmoncollect xrdcp xrdfs xrdreadv-eof xrdopen-denied

	write_security_fragment
	if [ "${MONCOLLECT_VOMS}" = 1 ]; then
		setup_moncollect_voms
	fi

	# When python3 is available, start the mock OTLP receiver and point the
	# collector at it (also enabling --spans so the traces export is exercised).
	OTLP_ARGS=""
	if command -v python3 >/dev/null 2>&1; then
		: > "${OTLP_OUT}"
		python3 "${SOURCE_DIR}/otlp_mock.py" "${OTLP_PORT}" "${OTLP_OUT}" \
		        > "${PWD}/${NAME}/otlp.log" 2>&1 < /dev/null &
		echo $! > "${OTLP_PID}"
		disown 2>/dev/null || true
		# --otlp-token exercises the bearer-auth path; the token is read from a
		# file (@<path>) so the secret is not passed on the command line.
		printf 'secrettoken123' > "${PWD}/${NAME}/otlp.token"
		OTLP_ARGS="--otlp-url http://127.0.0.1:${OTLP_PORT} --spans \
		           --otlp-token @${PWD}/${NAME}/otlp.token \
		           --cache-dir ${OTLP_CACHE}"
		sleep 1   # let it bind before the first export
	fi

	# Start the collector before the server so the f-stream destination has a
	# listener. The PID file lands in ${NAME}/ so the harness teardown kills it.
	# Redirect all of its fds (and detach) so it does not inherit and hold open
	# the test runner's stdout pipe, which would block ctest.
	# --dataset captures the per-run mktemp leaf (test-XXXXXX) of the paths the
	# test transfers, standing in for an experiment dataset name.
	: > "${COLLECTOR_OUT}"
	xrdmoncollect -p "${COLLECTOR_PORT}" -o "${COLLECTOR_OUT}" \
	              --flush-secs 1 --flush-count 1 ${OTLP_ARGS} \
	              --dataset '/(test-[A-Za-z0-9]+)/' \
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
	#    second until the document is observed (tolerates UDP loss). XRD_SITE is
	#    advertised by the client at login and surfaces as client.site.
	export XRD_SITE=CLIENT-TEST-SITE
	drive_until '"xrootd.operation_state":"Successful"' "successful transfer document" \
		"xrdcp -f '${TMPDIR}/ok.ref' '${HOST}/${TMPDIR}/ok.ref' \
		 && xrdcp -f '${HOST}/${TMPDIR}/ok.ref' '${TMPDIR}/ok.dat'"

	# The client-advertised site must travel to the collector as client.site.
	assert grep -Eq '"xrootd.client.site":"CLIENT-TEST-SITE"' "${COLLECTOR_OUT}"

	# The collector runs co-located with the server, so the monitor datagrams
	# arrive from the loopback address. The resource's server.address (the one
	# canonical server-name field) must carry a real identity (the local FQDN
	# substituted for loopback, or the '=' ident host) and must never be a
	# loopback literal or name.
	assert grep -Eq '"server.address":"[^"]+"' "${COLLECTOR_OUT}"
	assert_failure grep -Eq '"server\.address":"(localhost[^"]*|127\.[0-9.]+|::1|::ffff:127\.[0-9.]+)"' "${COLLECTOR_OUT}"

	# client.address carries the server-resolved client name when one is known,
	# with the numeric "&a=" IP as the fallback (and as network.peer.address
	# when the name wins). It must never be a loopback literal or name (the
	# loopback client is renamed to this host's public identity). There is no
	# separate xrootd.client.host field.
	assert grep -Eq '"client.address":"[^"]+"' "${COLLECTOR_OUT}"
	assert_failure grep -Eq '"client.address":"(localhost[^"]*|127\.[0-9.]+|::1|::ffff:127\.[0-9.]+)"' "${COLLECTOR_OUT}"
	assert_failure grep -q '"xrootd.client.host"' "${COLLECTOR_OUT}"

	# all.sitename (common.cfg sets it to the instance name) must surface
	# verbatim as the server resource's site.
	assert grep -Eq '"xrootd.server.site":"moncollect"' "${COLLECTOR_OUT}"

	# file.path is decomposed into the semconv file.directory, and the
	# --dataset capture group surfaces as xrootd.dataset.
	assert grep -Fq "\"file.directory\":\"${TMPDIR}\"" "${COLLECTOR_OUT}"
	assert grep -Eq '"xrootd.dataset":"test-[A-Za-z0-9]+"' "${COLLECTOR_OUT}"

	# OTLP export (when the mock receiver is running): the collector must POST an
	# OTLP logs export to /v1/logs (resourceLogs envelope with typed KeyValue
	# attributes) and, with --spans, a traces export to /v1/traces.
	if [ -f "${OTLP_PID}" ]; then
		for _ in $(seq 1 30); do
			grep -q 'resourceLogs' "${OTLP_OUT}" 2>/dev/null && break
			xrdcp -f "${TMPDIR}/ok.ref" "${HOST}/${TMPDIR}/ok.ref" >/dev/null 2>&1 || true
			sleep 1
		done
		assert grep -q '^/v1/logs ' "${OTLP_OUT}"
		assert grep -q '"resourceLogs"' "${OTLP_OUT}"
		assert grep -q '"key":"xrootd.operation_state"' "${OTLP_OUT}"
		# the bearer token (read from @file) must reach the endpoint
		assert grep -q '^authz /v1/logs Bearer secrettoken123' "${OTLP_OUT}"
		for _ in $(seq 1 15); do
			grep -q 'resourceSpans' "${OTLP_OUT}" 2>/dev/null && break; sleep 1
		done
		assert grep -q '^/v1/traces ' "${OTLP_OUT}"
		assert grep -q '"resourceSpans"' "${OTLP_OUT}"
		# --cache-dir gives the OTLP sink on-failure durability: the per-signal
		# cache subdirectories are created at startup (logs and traces replay to
		# different endpoints, so they cache separately).
		assert test -d "${OTLP_CACHE}/otlp-logs"
		assert test -d "${OTLP_CACHE}/otlp-traces"
		echo "found: OTLP logs + traces export (with disk cache)"
	fi

	# With VOMS, the proxy's fake VOMS attribute certificate must surface on the
	# monitoring stream: gsi extracts it into XrdSecEntity.vorg, the server emits
	# it in the MAPUSER record ("&o="), and the collector reports it as user.vo.
	# Re-drive uploads until a transfer document carries the VO (tolerates the
	# u-record racing the close under UDP), then check the role too.
	if [ "${MONCOLLECT_VOMS}" = 1 ]; then
		drive_until '"wlcg.vo":"dteam"' "VO surfaced on transfer document" \
			"xrdcp -f '${TMPDIR}/ok.ref' '${HOST}/${TMPDIR}/ok.ref'"
		assert grep -Eq '"wlcg.role":"production"' "${COLLECTOR_OUT}"
	fi

	# 2. A failed open: reading a nonexistent file fails before any close, so
	#    the server emits a terminal isError record -> operation_state "Failed".
	drive_until '"xrootd.operation_state":"Failed"' "failed-open document" \
		"xrdcp '${HOST}/${TMPDIR}/does-not-exist.root' '${TMPDIR}/x.dat'"

	# The failed-open report must, on a SINGLE document, name the missing file
	# and carry category "open" with the *specific* server-side reason (the real
	# "no such file or directory", not merely a non-empty string) and the kXR
	# error code (kXR_NotFound = 3011). Pin the checks to one record so they
	# cannot pass by matching fields spread across different documents.
	open_doc=$(grep -E '"file.path":"[^"]*does-not-exist\.root"' "${COLLECTOR_OUT}" \
		| grep -E '"xrootd.operation_state":"Failed"' | head -n1)
	test -n "${open_doc}" || error "no failed-open transfer document found"
	assert grep -Eq '"error.type":"open"' <<<"${open_doc}"
	assert grep -Eq '"xrootd.error.code":3011' <<<"${open_doc}"
	assert grep -Eq '"error.message":"Unable to open[^"]*no such file or directory"' \
		<<<"${open_doc}"

	# 3. A mid-transfer read error: a vector read past EOF fails on the server,
	#    which records the terminal error on the file so its close reports
	#    operation_state "Failed" with error_category "read". xrdreadv-eof opens
	#    the existing file and issues the failing readv, then closes.
	drive_until '"error.type":"read"' "failed-readv close document" \
		"xrdreadv-eof '${HOST}/${TMPDIR}/ok.ref'"

	# As with the open failure, verify the specific reason on a single document:
	# the readv-past-EOF close must report category "read" with the server's
	# ESPIPE reason (kXR_FSError = 3005), not just any error. The strerror text
	# differs by libc: "illegal seek" (glibc) vs "invalid seek" (musl).
	readv_doc=$(grep -E '"xrootd.app.name":"xrdreadv-eof"' "${COLLECTOR_OUT}" \
		| grep -E '"xrootd.operation_state":"Failed"' | head -n1)
	test -n "${readv_doc}" || error "no failed-readv transfer document found"
	assert grep -Eq '"error.type":"read"' <<<"${readv_doc}"
	assert grep -Eq '"xrootd.error.code":3005' <<<"${readv_doc}"
	assert grep -Eq '"error.message":"Unable to readv[^"]*(illegal|invalid) seek"' \
		<<<"${readv_doc}"

	# 4. A lock-denied open: a second writer of an already-open file is rejected
	#    by the server's file-lock manager (kXR_FileLocked) *before* the
	#    filesystem open, a branch of do_Open that sends the error directly and
	#    so used to bypass the terminal-error report. xrdopen-denied opens the
	#    existing file for write twice; the second open must surface as a failed
	#    open with the lock reason.
	drive_until '"error.message":"[^"]*open denied' "lock-denied open document" \
		"xrdopen-denied '${HOST}/${TMPDIR}/ok.ref'"

	locked_doc=$(grep -E '"error.message":"[^"]*open denied' "${COLLECTOR_OUT}" \
		| head -n1)
	test -n "${locked_doc}" || error "no lock-denied open document found"
	assert grep -Eq '"error.type":"open"' <<<"${locked_doc}"
	assert grep -Eq '"xrootd.error.code":3003' <<<"${locked_doc}"
	assert grep -Eq '"error.message":"[^"]*is already opened by[^"]*open denied' \
		<<<"${locked_doc}"

	echo "collector documents:"
	cat "${COLLECTOR_OUT}"
}
