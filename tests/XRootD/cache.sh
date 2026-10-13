#!/usr/bin/env bash
#
# Core XCache read path: whole-file mode, prefetching off, one origin.
# What this asserts is where the bytes came from, not just that they arrived --
# returning the right bytes is what a plain proxy would also do.

export XrdSecPROTOCOL=host

# shellcheck source=/dev/null
source "$(dirname "${BASH_SOURCE[0]}")/cachelib.sh"

# Must match pfc.blocksize in cache.cfg.
PFC_BLOCKSIZE=$((128 * 1024))

# Must match the destination port of the xrootd.mongstream line in cache.cfg.
PFC_GSTREAM_PORT=7097

# Test file size, in whole cache blocks.
PFC_NBLOCKS=32

function setup_cache() {
	require_commands diff
	cache_setup
}

# Size reported by xrdfs stat for $2 on server $1.
function stat_size() {
	xrdfs "$1" stat "$2" 2>/dev/null | sed -n 's/^Size:[[:space:]]*\([0-9]*\).*/\1/p'
}

function test_cache() {
	ensure_gstream_collector

	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	local size=$((PFC_BLOCKSIZE * PFC_NBLOCKS))

	#---------------------------------------------------------------------------
	echo "--- a file read through the cache is byte-identical to the origin"
	#---------------------------------------------------------------------------

	assert xrdcp -f "${SOURCE_DIR}"/cache.cfg "${ORIGIN_HOST}//cache.cfg"
	assert xrdcp -f "${ORIGIN_HOST}"//cache.cfg cache.origin.cfg
	assert diff -u "${SOURCE_DIR}"/cache.cfg cache.origin.cfg

	assert xrdcp -f "${HOST}"//cache.cfg host.cache.cfg
	assert diff -u "${SOURCE_DIR}"/cache.cfg host.cache.cfg

	#---------------------------------------------------------------------------
	echo "--- cold read fetches the whole file, warm read touches no origin"
	#---------------------------------------------------------------------------

	local lfn=pfc-hitmiss.dat
	local cinfo rec hit miss bypass
	cinfo="$(cinfo_path ${lfn})"

	make_origin_file hitmiss.dat "${lfn}" "${PFC_NBLOCKS}"

	if [[ -e "${cinfo}" ]]; then
		error "cache already holds ${lfn} before the cold read"
	fi

	assert xrdcp -f "${HOST}//${lfn}" cold.dat
	assert cmp hitmiss.dat cold.dat

	rec="$(wait_for_gstream_close "${lfn}" 1)"
	assert_eq "${size}" "$(json_int "${rec}" b_todisk)" \
		"cold read should fetch the whole file from the origin"
	assert_eq "0" "$(json_int "${rec}" b_prefetch)" \
		"cold read should fetch nothing speculatively, pfc.prefetch is 0"
	assert_eq "${PFC_NBLOCKS}" "$(json_int "${rec}" n_blks_done)" \
		"cold read should leave every block downloaded"

	wait_for_access_record "${cinfo}" 1
	assert_eq "${PFC_NBLOCKS} ${PFC_NBLOCKS} complete" "$(cinfo_blocks "${cinfo}")" \
		"cold read should leave the file complete in the cache"

	# B_hit/B_miss cannot carry "came over the network" -- see the note at the
	# top of cachelib.sh -- but they do have to add up and account for
	# everything, with nothing slipping past the cache.
	read -r hit miss bypass <<< "$(cinfo_access "${cinfo}" 1)"
	assert_eq "${size}" "$((hit + miss))" "cold read: hit plus miss should be the file size"
	assert_eq "0" "${bypass}" "cold read should not bypass the cache"

	assert xrdcp -f "${HOST}//${lfn}" warm.dat
	assert cmp hitmiss.dat warm.dat

	assert_fetched "${lfn}" 2 0 "warm read should not fetch anything from the origin"

	wait_for_access_record "${cinfo}" 2
	assert_eq "${size} 0 0" "$(cinfo_access "${cinfo}" 2)" \
		"warm read should be all hit (B_hit B_miss B_bypass)"

	#---------------------------------------------------------------------------
	echo "--- a small read pulls one whole block, and only once"
	#---------------------------------------------------------------------------

	# The cache is block-granular: a 4 kB read of an uncached file has to fetch
	# the whole block it falls in, and nothing more. Reading the same 4 kB again
	# must then go nowhere near the origin.

	local plfn=pfc-partial.dat
	local pcinfo
	pcinfo="$(cinfo_path ${plfn})"

	make_origin_file partial.dat "${plfn}" "${PFC_NBLOCKS}"

	assert xrdpfc-read read "${HOST}//${plfn}" part1.dat 0:4096
	assert dd if=partial.dat of=part1.ref bs=4096 count=1 status=none
	assert cmp part1.dat part1.ref

	assert_fetched "${plfn}" 1 "${PFC_BLOCKSIZE}" \
		"a 4 kB read should pull exactly the one block it falls in"

	wait_for_access_record "${pcinfo}" 1
	assert_eq "${PFC_NBLOCKS} 1 incomplete" "$(cinfo_blocks "${pcinfo}")" \
		"after a 4 kB read the file should be one block in and incomplete"

	assert xrdpfc-read read "${HOST}//${plfn}" part2.dat 0:4096
	assert cmp part2.dat part1.ref
	assert_fetched "${plfn}" 2 0 "re-reading a cached block should not reach the origin"

	#---------------------------------------------------------------------------
	echo "--- a vector read fetches only the blocks its chunks land in"
	#---------------------------------------------------------------------------

	# Scattered reads are the case XCache exists for. Three 4 kB chunks in
	# blocks 0, 8 and 24: block 0 is already cached from above, so exactly two
	# blocks should come over the network, and the returned bytes must be the
	# three ranges concatenated in order.

	local b8=$((PFC_BLOCKSIZE * 8))
	local b24=$((PFC_BLOCKSIZE * 24))

	assert xrdpfc-read readv "${HOST}//${plfn}" vec.dat 0:4096 "${b8}":4096 "${b24}":4096

	assert dd if=partial.dat of=vec.ref bs=4096 count=1 status=none
	dd if=partial.dat bs=4096 skip=$((b8 / 4096)) count=1 status=none >> vec.ref
	dd if=partial.dat bs=4096 skip=$((b24 / 4096)) count=1 status=none >> vec.ref
	assert cmp vec.dat vec.ref

	assert_fetched "${plfn}" 3 $((2 * PFC_BLOCKSIZE)) \
		"a vector read should fetch only the blocks it has chunks in"

	wait_for_access_record "${pcinfo}" 3
	assert_eq "${PFC_NBLOCKS} 3 incomplete" "$(cinfo_blocks "${pcinfo}")" \
		"the vector read should have added two blocks"

	# Which blocks, not just how many.
	local expected_map
	expected_map="$(python3 -c "
m = ['.'] * ${PFC_NBLOCKS}
for i in (0, 8, 24): m[i] = 'x'
print(''.join(m))")"
	assert_eq "${expected_map}" "$(cinfo_block_map "${pcinfo}")" \
		"only the blocks the chunks landed in should be present"

	#---------------------------------------------------------------------------
	echo "--- concurrent readers of the same block fetch it once between them"
	#---------------------------------------------------------------------------

	# Four readers opening the same uncached file at once and reading the same
	# two blocks. They share one File object, so the origin should see those
	# two blocks once, not four times, and every reader must get the same bytes.

	local clfn=pfc-concurrent.dat
	local cbytes=$((PFC_BLOCKSIZE * 2))

	make_origin_file concurrent.dat "${clfn}" "${PFC_NBLOCKS}"

	assert xrdpfc-read multi "${HOST}//${clfn}" conc.dat 4 "${cbytes}"
	assert dd if=concurrent.dat of=conc.ref bs="${PFC_BLOCKSIZE}" count=2 status=none
	assert cmp conc.dat conc.ref

	assert_eq "${cbytes}" "$(sum_gstream_todisk "${clfn}")" \
		"four concurrent readers of the same two blocks should fetch them once"

	#---------------------------------------------------------------------------
	echo "--- stat reports the origin's size even when little of the file is cached"
	#---------------------------------------------------------------------------

	# pfc-partial.dat is three blocks of thirty-two on disk at this point. The
	# cache must still report the real length: a client that believed the size
	# of the local sparse copy would read a truncated file.

	local origin_size cache_size
	origin_size="$(stat_size "${ORIGIN_HOST}" "/${plfn}")"
	cache_size="$(stat_size "${HOST}" "/${plfn}")"

	assert_eq "${size}" "${origin_size}" "the origin should report the full size"
	assert_eq "${origin_size}" "${cache_size}" \
		"stat through the cache should report the origin's size, not what is on disk"

	# And it is still incomplete, so this was not a full copy in disguise.
	assert_eq "${PFC_NBLOCKS} 3 incomplete" "$(cinfo_blocks "${pcinfo}")" \
		"the partially cached file should still be partial"

	#---------------------------------------------------------------------------
	echo "--- writing through the cache is refused, cleanly"
	#---------------------------------------------------------------------------

	# The proxy is read-only. A write has to fail outright rather than land in
	# the cache, or worse, half-land and be served later as if it were real.

	assert dd if=/dev/urandom of=upload.dat bs=4096 count=1 status=none
	assert_failure xrdcp -f upload.dat "${HOST}//pfc-uploaded.dat"

	if [[ -e "$(cache_store)/pfc-uploaded.dat" ]] ||
	   [[ -e "$(cinfo_path pfc-uploaded.dat)" ]]; then
		error "a refused write left state in the cache"
	fi
	assert_failure xrdfs "${ORIGIN_HOST}" stat /pfc-uploaded.dat

	#---------------------------------------------------------------------------
	echo "--- a missing file fails and leaves nothing behind in the cache"
	#---------------------------------------------------------------------------

	# The cache must pass the origin's error through and not create state for a
	# file that does not exist.

	assert_failure xrdcp -f "${HOST}//pfc-does-not-exist.dat" missing.dat

	if [[ -e "$(cache_store)/pfc-does-not-exist.dat" ]] ||
	   [[ -e "$(cinfo_path pfc-does-not-exist.dat)" ]]; then
		error "cache created state for a file that does not exist on the origin"
	fi
}
