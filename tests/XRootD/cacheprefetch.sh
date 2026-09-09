#!/usr/bin/env bash
#
# Prefetching. The prefetcher is what makes a cache useful for sequential
# reading, and it is also what makes the b_hit/b_miss pair unusable as a
# measure of network traffic -- see the note in cachelib.sh.

export XrdSecPROTOCOL=host

# shellcheck source=/dev/null
source "$(dirname "${BASH_SOURCE[0]}")/cachelib.sh"

PFC_BLOCKSIZE=$((128 * 1024))
PFC_GSTREAM_PORT=7103
PFC_NBLOCKS=32

function setup_cacheprefetch() {
	cache_setup
}

function test_cacheprefetch() {
	ensure_gstream_collector

	local lfn=pfc-prefetch.dat
	local size=$((PFC_BLOCKSIZE * PFC_NBLOCKS))
	local cinfo rec todisk prefetched
	cinfo="$(cinfo_path ${lfn})"

	make_origin_file prefetch.dat "${lfn}" "${PFC_NBLOCKS}"

	#---------------------------------------------------------------------------
	echo "--- a small read pulls more than it asked for, and it is prefetch"
	#---------------------------------------------------------------------------

	# With pfc.prefetch 8 a 4 kB read should drag further blocks in behind it.
	# Without prefetching the same read fetches exactly one block -- that is
	# asserted in cache.sh, so the two tests bracket the behaviour.

	assert xrdpfc-read read "${HOST}//${lfn}" pf-small.dat 0:4096
	assert dd if=prefetch.dat of=pf-small.ref bs=4096 count=1 status=none
	assert cmp pf-small.dat pf-small.ref

	rec="$(wait_for_gstream_close "${lfn}" 1)"
	todisk="$(json_int "${rec}" b_todisk)"
	prefetched="$(json_int "${rec}" b_prefetch)"

	if [[ "${todisk}" -le "${PFC_BLOCKSIZE}" ]]; then
		error "prefetch is on but a 4 kB read fetched only ${todisk} bytes;" \
		      "expected more than one ${PFC_BLOCKSIZE} byte block"
	fi
	if [[ "${prefetched}" -le 0 ]]; then
		error "prefetch is on but b_prefetch is ${prefetched}"
	fi
	if [[ "${prefetched}" -gt "${todisk}" ]]; then
		error "b_prefetch ${prefetched} exceeds b_todisk ${todisk};" \
		      "prefetched bytes are a subset of what was fetched"
	fi

	#---------------------------------------------------------------------------
	echo "--- prefetch never fetches past the end of the file"
	#---------------------------------------------------------------------------

	# Read the whole file. However eagerly the prefetcher ran, the origin
	# cannot have been asked for more than the file holds, and every block has
	# to end up present exactly once.

	local pfull=pfc-prefetch-full.dat
	local pfcinfo
	pfcinfo="$(cinfo_path ${pfull})"

	make_origin_file prefetch-full.dat "${pfull}" "${PFC_NBLOCKS}"

	assert xrdcp -f "${HOST}//${pfull}" pf-full.dat
	assert cmp prefetch-full.dat pf-full.dat

	assert_fetched "${pfull}" 1 "${size}" \
		"a full read with prefetching should fetch the file exactly once"

	wait_for_access_record "${pfcinfo}" 1
	assert_eq "${PFC_NBLOCKS} ${PFC_NBLOCKS} complete" "$(cinfo_blocks "${pfcinfo}")" \
		"the file should be complete after a full read"

	#---------------------------------------------------------------------------
	echo "--- a re-read of a fully prefetched file goes nowhere"
	#---------------------------------------------------------------------------

	assert xrdcp -f "${HOST}//${pfull}" pf-full2.dat
	assert cmp prefetch-full.dat pf-full2.dat
	assert_fetched "${pfull}" 2 0 "re-reading a complete file should not reach the origin"
}
