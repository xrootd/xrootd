#!/usr/bin/env bash
# Exercises the xrdmoncollect INI configuration file: a value is read from the
# file, a command-line option overrides it, and malformed/missing files are
# reported. Only deterministic, non-binding paths are checked (an out-of-range
# port makes the validated start-up fail fast without opening a socket).

set -u

BIN=${1:?usage: config_test.sh <path-to-xrdmoncollect>}
TMP=$(mktemp -d)
trap 'rm -rf "${TMP}"' EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }

# Start the collector with the given arguments and check that it is still
# running (i.e. got past parsing/validation and into the receive loop) shortly
# afterwards, then shut it down. This deliberately avoids timeout(1)'s exit
# code: the 124-on-timeout convention is GNU-specific (busybox on Alpine
# propagates the child's status, and the collector exits 0 on SIGTERM).
starts() {
	local pid alive=0
	"${BIN}" "$@" & pid=$!
	sleep 1
	kill -0 "${pid}" 2>/dev/null && alive=1
	kill "${pid}" 2>/dev/null
	wait "${pid}" 2>/dev/null
	test "${alive}" -eq 1
}

# 1. A value from the file takes effect: an out-of-range port in the config is
#    applied and rejected by the same validation as a command-line port.
cat > "${TMP}/badport.cfg" <<EOF
[xrdmoncollect]
port = 70000
EOF
out=$("${BIN}" -c "${TMP}/badport.cfg" 2>&1)
test $? -ne 0 || fail "out-of-range config port should fail"
grep -q "valid -p" <<<"${out}" || fail "expected port validation error, got: ${out}"

# 2. A command-line option overrides the file: a valid -p replaces the bad
#    config port, so validation passes and the collector starts (it would
#    otherwise bind and run, so 'starts' stops it after probing).
starts -c "${TMP}/badport.cfg" -p 9931 \
	|| fail "CLI -p should override config and start"

# 3. A malformed config is a hard error naming the line.
printf '[xrdmoncollect\nport = 9931\n' > "${TMP}/bad.cfg"
out=$("${BIN}" -c "${TMP}/bad.cfg" 2>&1)
test $? -ne 0 || fail "malformed config should fail"
grep -qi "parse error" <<<"${out}" || fail "expected parse error, got: ${out}"

# 4. An explicitly named but unreadable config is a hard error.
out=$("${BIN}" -c "${TMP}/does-not-exist.cfg" 2>&1)
test $? -ne 0 || fail "missing -c file should fail"
grep -qi "cannot open" <<<"${out}" || fail "expected open error, got: ${out}"

# 5. A valid config is honoured: the file alone supplies the port and the
#    collector starts (binds), which we again stop quickly.
cat > "${TMP}/ok.cfg" <<EOF
[xrdmoncollect]
port = 9932
no-resolve = true
max-memory = 64M
EOF
starts -c "${TMP}/ok.cfg" || fail "valid config should start the collector"

echo "ok"
