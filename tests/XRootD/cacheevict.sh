#!/usr/bin/env bash
#
# only-if-cached and eviction: the two ways a client asks the cache about, or
# changes, what it is holding, without going to the origin for the answer.

export XrdSecPROTOCOL=host

# shellcheck source=/dev/null
source "$(dirname "${BASH_SOURCE[0]}")/cachelib.sh"

PFC_BLOCKSIZE=$((128 * 1024))
PFC_GSTREAM_PORT=7101
PFC_NBLOCKS=32

function setup_cacheevict() {
	cache_setup
}

function test_cacheevict() {
	ensure_gstream_collector

	local lfn=pfc-evict.dat
	local size=$((PFC_BLOCKSIZE * PFC_NBLOCKS))
	local cinfo
	cinfo="$(cinfo_path ${lfn})"

	make_origin_file evict.dat "${lfn}" "${PFC_NBLOCKS}"

	#---------------------------------------------------------------------------
	echo "--- only-if-cached refuses a file the cache does not hold"
	#---------------------------------------------------------------------------

	# The open must fail rather than silently falling back to the origin --
	# that is the entire point of the flag for a client that is trying to
	# avoid a WAN transfer.
	assert_failure xrdcp -f "${HOST}//${lfn}?only-if-cached" oic-cold.dat

	if [[ -e "${cinfo}" ]]; then
		error "a refused only-if-cached open should not have created cache state"
	fi

	#---------------------------------------------------------------------------
	echo "--- a partly cached file still does not count as cached"
	#---------------------------------------------------------------------------

	# pfc.onlyifcached minfrac 1.0: one block present is not enough.
	assert xrdpfc-read read "${HOST}//${lfn}" evict-part.dat 0:4096
	wait_for_access_record "${cinfo}" 1
	assert_eq "${PFC_NBLOCKS} 1 incomplete" "$(cinfo_blocks "${cinfo}")" \
		"the 4 kB read should have left the file incomplete"

	assert_failure xrdcp -f "${HOST}//${lfn}?only-if-cached" oic-part.dat

	#---------------------------------------------------------------------------
	echo "--- once fully cached it is served, without touching the origin"
	#---------------------------------------------------------------------------

	assert xrdcp -f "${HOST}//${lfn}" evict-full.dat
	assert cmp evict.dat evict-full.dat
	wait_for_access_record "${cinfo}" 2
	assert_eq "${PFC_NBLOCKS} ${PFC_NBLOCKS} complete" "$(cinfo_blocks "${cinfo}")" \
		"the full read should have completed the file"

	assert xrdcp -f "${HOST}//${lfn}?only-if-cached" oic-warm.dat
	assert cmp evict.dat oic-warm.dat

	# Assert on the running total rather than on a numbered record. Between
	# them the reads above have pulled the file exactly once; if the
	# only-if-cached read had gone to the origin the total would have grown.
	# A read that never constructs a File emits no record at all, and the
	# refused only-if-cached opens may or may not have bumped the access count
	# depending on whether the file was still active, so neither the record
	# count nor its position in the log can be pinned down.
	assert_eq "${size}" "$(sum_gstream_todisk "${lfn}")" \
		"an only-if-cached read should never reach the origin"

	#---------------------------------------------------------------------------
	echo "--- evict removes both the data file and its cinfo"
	#---------------------------------------------------------------------------

	assert xrdfs "${HOST}" cache evict "/${lfn}"

	for _ in $(seq 50); do
		[[ -e "${cinfo}" ]] || break
		sleep 0.2
	done

	if [[ -e "${cinfo}" ]]; then
		error "evict left the cinfo behind: ${cinfo}"
	fi
	if [[ -e "$(cache_store)/${lfn}" ]]; then
		error "evict left the data file behind: $(cache_store)/${lfn}"
	fi

	# Evicting must not have touched the origin's copy.
	assert xrdcp -f "${ORIGIN_HOST}//${lfn}" evict-origin.dat
	assert cmp evict.dat evict-origin.dat

	#---------------------------------------------------------------------------
	echo "--- after eviction the cache is cold again"
	#---------------------------------------------------------------------------

	assert_failure xrdcp -f "${HOST}//${lfn}?only-if-cached" oic-evicted.dat

	assert xrdcp -f "${HOST}//${lfn}" evict-refetch.dat
	assert cmp evict.dat evict-refetch.dat

	# The origin has now served the file twice over: once before the eviction
	# and once after.
	assert_eq "$((2 * size))" "$(sum_gstream_todisk "${lfn}")" \
		"the read after eviction should fetch the whole file again"
}
