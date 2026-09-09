#!/usr/bin/env bash
#
# Shared helpers for the XCache (XrdPfc) tests. Sourced by cache*.sh, each of
# which drives one cache configuration against the XRootD::host fixture.
#
# Two independent oracles are available, and they answer different questions:
#
#   * the pfc g-stream file_close record -- b_todisk is exactly how many bytes
#     came over the network on that open, and b_prefetch how much of that was
#     speculative. Both are counted per block as blocks are written out, so
#     they do not move with the client's request size.
#
#   * the .cinfo file, via xrdpfc_print -- the state left on disk: which blocks
#     are present, whether the file is complete, and the access history.
#
# Do not assert on B_hit/B_miss to mean "came from the network". PFC scores a
# block as missed only for the request that created it, so a second request
# landing on the same in-flight block reads as a hit, and with prefetching on a
# cold read of an uncached file reports 100% hit. That is upstream issue #2366.
# b_todisk is the field that states it correctly.

# Defaults. A config that differs must set these before calling cache_setup.
: "${PFC_BLOCKSIZE:=$((128 * 1024))}"

ORIGIN_HOST="root://localhost:5094"

#-------------------------------------------------------------------------------
# Paths. setup and run are separate processes, so derive rather than export.
#-------------------------------------------------------------------------------

# Local store of the cache: common.cfg points oss.localroot at what test.sh
# exports as REMOTE_DIR, so <lfn> and <lfn>.cinfo land there.
function cache_store() {
	echo "${REMOTE_DIR}"
}

function cinfo_path() {
	echo "$(cache_store)/$1.cinfo"
}

function gstream_log() {
	echo "${PWD}/${NAME}/gstream.json"
}

#-------------------------------------------------------------------------------
# Setup
#-------------------------------------------------------------------------------

# Start the g-stream collector. Call from setup_<name> with PFC_GSTREAM_PORT
# set to the destination port of the xrootd.mongstream line in the config.
function cache_setup() {
	require_commands cmp dd python3 xrdpfc-read xrdpfc_print

	# A test that asserts on b_todisk sets PFC_GSTREAM_PORT and gets a
	# collector; one that only reads the filesystem leaves it unset and does
	# without, so it needs neither a free UDP port nor a working g-stream.
	[[ -n "${PFC_GSTREAM_PORT:-}" ]] || return 0

	rm -f "$(gstream_log)"
	start_gstream_collector
}

# The helper detaches itself and writes its own pid file: a plain background job
# would be killed with the rest of this test's process group as soon as ctest
# reaps the setup step, and one that merely survived would hang the run by
# holding ctest's output pipe open. It returns once the port is bound, so the
# server cannot come up before the collector is listening. test.sh's generic
# teardown kills anything that left a pid file behind.
function start_gstream_collector() {
	assert python3 "${SOURCE_DIR}/utils/gstream_recv.py" \
		"${PFC_GSTREAM_PORT}" "$(gstream_log)" "${PWD}/${NAME}/gstream.pid"
}

# True while the collector started at setup is still running.
function gstream_collector_alive() {
	local pidfile="${PWD}/${NAME}/gstream.pid"
	[[ -s "${pidfile}" ]] && kill -0 "$(cat "${pidfile}")" 2>/dev/null
}

# Call at the top of a test body that asserts on the g-stream.
#
# The collector starts at setup, but ctest runs every setup first and the test
# bodies long afterwards -- in a full run that gap is eighty-odd tests and about
# ten minutes. CI loses one of these idle processes now and then, and since
# nothing restarts it, every g-stream assertion in that one test fails while the
# rest of the job is fine; the victim moved between platforms and tests on each
# run. Starting another one works: a UDP listener that comes back keeps
# receiving. The server does drop one datagram when it returns, the one whose
# send reports the ICMP queued from while the port was dead, but nothing is sent
# between setup and here, so there is none pending.
function ensure_gstream_collector() {
	[[ -n "${PFC_GSTREAM_PORT:-}" ]] || return 0
	gstream_collector_alive && return 0

	echo "g-stream collector is gone, starting another"
	start_gstream_collector
}

#-------------------------------------------------------------------------------
# Test data
#-------------------------------------------------------------------------------

# Size of a file in bytes. GNU stat -c%s and BSD stat -f%z disagree; wc -c is
# in POSIX and needs no branch on uname.
function file_bytes() {
	wc -c < "$1"
}

# make_origin_file <local> <lfn> <n_blocks> -- random file of n_blocks whole
# cache blocks, put on the origin. Whole blocks keep the expected byte counts
# free of rounding.
function make_origin_file() {
	assert dd if=/dev/urandom of="$1" bs="${PFC_BLOCKSIZE}" count="$3" status=none
	assert xrdcp -fs "$1" "${ORIGIN_HOST}//$2"
}

#-------------------------------------------------------------------------------
# The g-stream oracle
#-------------------------------------------------------------------------------

# Integer field $2 of JSON record $1.
function json_int() {
	echo "$1" | sed -n "s/.*\"$2\":\(-\{0,1\}[0-9]\{1,\}\).*/\1/p"
}

# The file_close record for lfn $1 at access_cnt $2. The record is only emitted
# when the cache closes the file, which happens after the client has gone, and
# the g-stream is flushed on a timer on top of that -- so wait, do not race.
function wait_for_gstream_close() {
	local rec
	for _ in $(seq 100); do
		rec="$(grep '"event":"file_close"' "$(gstream_log)" 2>/dev/null |
		       grep "\"lfn\":\"/$1\"" | grep "\"access_cnt\":$2," | tail -1)" || true
		if [[ -n "${rec}" ]]; then
			echo "${rec}"
			return 0
		fi
		sleep 0.2
	done
	# Say whether the log is empty or merely missing this record: the first
	# means the collector never received anything, the second that the cache
	# did not close the file when expected. They need different fixes.
	error "timed out after 20 s waiting for g-stream file_close of /$1" \
	      "at access_cnt $2; $(gstream_diagnosis)"
}

# Why might a record be missing? A dead collector and a cache that never closed
# the file need different fixes, and the bare timeout cannot tell them apart.
function gstream_diagnosis() {
	local n_all alive="no"
	n_all="$(grep -c '"event":"file_close"' "$(gstream_log)" 2>/dev/null || echo 0)"
	gstream_collector_alive && alive="yes"
	echo "collector alive: ${alive}, log holds ${n_all} file_close records"
}

# Total b_todisk over every file_close record for lfn $1 so far -- the bytes
# the origin has served for it since the cache started.
#
# Cumulative rather than per-record on purpose. Records are emitted when the
# cache closes a file and are then flushed on a timer, so which record a given
# read lands in, and when it appears in the log, both move around; a running
# total does not. Waits for the first record, then lets stragglers land.
function sum_gstream_todisk() {
	local rec found=""
	for _ in $(seq 100); do
		found="$(grep '"event":"file_close"' "$(gstream_log)" 2>/dev/null |
		         grep "\"lfn\":\"/$1\"" | head -1)" || true
		[[ -n "${found}" ]] && break
		sleep 0.2
	done
	if [[ -z "${found}" ]]; then
		error "timed out after 20 s waiting for any g-stream file_close of /$1;" \
		      "$(gstream_diagnosis)"
	fi
	sleep 3

	local total=0
	while read -r rec; do
		[[ -n "${rec}" ]] || continue
		total=$((total + $(json_int "${rec}" b_todisk)))
	done < <(grep '"event":"file_close"' "$(gstream_log)" 2>/dev/null |
	         grep "\"lfn\":\"/$1\"" || true)

	echo "${total}"
}

# assert_fetched <lfn> <access_cnt> <expected b_todisk> <message>
function assert_fetched() {
	local rec
	rec="$(wait_for_gstream_close "$1" "$2")"
	assert_eq "$3" "$(json_int "${rec}" b_todisk)" "$4"
}

#-------------------------------------------------------------------------------
# The cinfo oracle
#-------------------------------------------------------------------------------

# "<n_blocks> <n_downloaded> <state>"
function cinfo_blocks() {
	xrdpfc_print -u B "$1" |
	sed -n 's/^file_size .*n_blocks \([0-9]*\), n_downloaded \([0-9]*\), state \([a-z]*\).*/\1 \2 \3/p'
}

function cinfo_n_acc() {
	xrdpfc_print -u B "$1" | sed -n 's/^Access records (N_acc_total=\([0-9]*\)).*/\1/p'
}

# "<B_hit> <B_miss> <B_bypass>" of access record $2, in bytes.
function cinfo_access() {
	xrdpfc_print -u B "$1" | awk -v rec="$2" '$1 == rec { print $(NF-2), $(NF-1), $NF; exit }'
}

# The block presence map as a string of x and . , one char per block.
function cinfo_block_map() {
	xrdpfc_print -u B -v "$1" |
	sed -n '/^printing /,/^Access records/p' | sed -n 's/^ *[0-9]\{1,\} \([x.]\{1,\}\)$/\1/p' | tr -d '\n'
}

function wait_for_access_record() {
	for _ in $(seq 100); do
		if [[ -f "$1" ]] && [[ "$(cinfo_n_acc "$1")" == "$2" ]]; then
			return 0
		fi
		sleep 0.2
	done
	error "timed out after 20 s waiting for access record $2 of $1"
}
