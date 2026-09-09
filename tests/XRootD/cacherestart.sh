#!/usr/bin/env bash
#
# Cache content has to survive a restart of the server. That means the cinfo
# written on close is readable again, and the resource monitor's startup scan
# picks up what is already on disk instead of ignoring or discarding it.

export XrdSecPROTOCOL=host

# shellcheck source=/dev/null
source "$(dirname "${BASH_SOURCE[0]}")/cachelib.sh"

PFC_BLOCKSIZE=$((128 * 1024))
PFC_GSTREAM_PORT=7109
PFC_NBLOCKS=32

function setup_cacherestart() {
	cache_setup
}

# Is the cache server answering? XRD_CONNECTIONRETRY=0 so that a probe against
# a server that is already gone fails at once instead of working through the
# client's reconnect window.
function server_is_up() {
	XRD_CONNECTIONRETRY=0 xrdfs "${HOST}" query config version >/dev/null 2>&1
}

# Stop and start the server with the same configuration and the same data
# directory. Deliberately not test.sh's setup(), which wipes the store -- the
# whole point here is that the store outlives the process.
function restart_server() {
	local pidfile="${PWD}/${NAME}/xrootd.pid"
	local pid
	pid="$(cat "${pidfile}")"

	kill -s TERM "${pid}"

	# Wait for the server to stop answering, rather than for its pid to
	# disappear. Watching the pid is what the first version did and it hung
	# for the full timeout on every CI platform: a pid is not a reliable
	# handle here, it can be recycled by another process in a busy container
	# and "ps <pid>" is not portable in that form anyway. Not answering is
	# also the condition that actually matters, since it is what releases the
	# port for the next server.
	for _ in $(seq 240); do
		server_is_up || break
		sleep 0.5
	done
	if server_is_up; then
		error "cache server ${pid} still answering 120 s after SIGTERM"
	fi

	assert xrootd -b -l xrootd.log -s xrootd.pid -c "${CONF}" -n "${NAME}"

	for _ in $(seq 240); do
		if server_is_up; then
			return 0
		fi
		sleep 0.5
	done
	error "cache server did not come back up after restart"
}

function test_cacherestart() {
	ensure_gstream_collector

	local complete=pfc-restart-full.dat
	local partial=pfc-restart-part.dat
	local size=$((PFC_BLOCKSIZE * PFC_NBLOCKS))
	local ccinfo pcinfo
	ccinfo="$(cinfo_path ${complete})"
	pcinfo="$(cinfo_path ${partial})"

	make_origin_file restart-full.dat "${complete}" "${PFC_NBLOCKS}"
	make_origin_file restart-part.dat "${partial}" "${PFC_NBLOCKS}"

	#---------------------------------------------------------------------------
	echo "--- populate the cache: one file complete, one deliberately partial"
	#---------------------------------------------------------------------------

	assert xrdcp -f "${HOST}//${complete}" restart-c1.dat
	assert cmp restart-full.dat restart-c1.dat
	assert_fetched "${complete}" 1 "${size}" "cold read should fetch the whole file"

	# Two chunks, in blocks 0 and 8, so the surviving state is a specific
	# pattern rather than just a count.
	local b8=$((PFC_BLOCKSIZE * 8))
	assert xrdpfc-read read "${HOST}//${partial}" restart-p1.dat 0:4096 "${b8}":4096
	assert_fetched "${partial}" 1 $((2 * PFC_BLOCKSIZE)) \
		"the partial read should have fetched two blocks"

	wait_for_access_record "${ccinfo}" 1
	wait_for_access_record "${pcinfo}" 1

	local expected_map
	expected_map="$(python3 -c "
m = ['.'] * ${PFC_NBLOCKS}
for i in (0, 8): m[i] = 'x'
print(''.join(m))")"
	assert_eq "${expected_map}" "$(cinfo_block_map "${pcinfo}")" \
		"blocks 0 and 8 should be the ones present before the restart"

	#---------------------------------------------------------------------------
	echo "--- restart the cache server"
	#---------------------------------------------------------------------------

	restart_server

	# On-disk state must be untouched by the restart itself.
	assert_eq "${PFC_NBLOCKS} ${PFC_NBLOCKS} complete" "$(cinfo_blocks "${ccinfo}")" \
		"the complete file should still be complete after the restart"
	assert_eq "${expected_map}" "$(cinfo_block_map "${pcinfo}")" \
		"the partial file should still hold exactly blocks 0 and 8"

	#---------------------------------------------------------------------------
	echo "--- the complete file is served from disk by the new process"
	#---------------------------------------------------------------------------

	assert xrdcp -f "${HOST}//${complete}" restart-c2.dat
	assert cmp restart-full.dat restart-c2.dat
	assert_fetched "${complete}" 2 0 \
		"after a restart a complete file should still be served without the origin"

	#---------------------------------------------------------------------------
	echo "--- the partial file keeps its blocks and only fetches what is missing"
	#---------------------------------------------------------------------------

	# Re-reading the two surviving blocks must cost nothing; the cache would
	# have to have forgotten them to go back to the origin.
	assert xrdpfc-read read "${HOST}//${partial}" restart-p2.dat 0:4096 "${b8}":4096
	assert cmp restart-p1.dat restart-p2.dat
	assert_fetched "${partial}" 2 0 \
		"blocks cached before the restart should still be on disk after it"

	# Reading the whole file now must fetch exactly the blocks that were
	# missing, not the whole file again.
	assert xrdcp -f "${HOST}//${partial}" restart-p3.dat
	assert cmp restart-part.dat restart-p3.dat
	assert_fetched "${partial}" 3 $(((PFC_NBLOCKS - 2) * PFC_BLOCKSIZE)) \
		"completing the file should fetch only the blocks that were missing"

	wait_for_access_record "${pcinfo}" 3
	assert_eq "${PFC_NBLOCKS} ${PFC_NBLOCKS} complete" "$(cinfo_blocks "${pcinfo}")" \
		"the file should now be complete"
}
