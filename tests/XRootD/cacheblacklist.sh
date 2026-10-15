#!/usr/bin/env bash
#
# pfc.decisionlib: the cache asks a plugin whether a file should be cached at
# all. A refusal has to proxy the data through untouched and leave no state
# behind -- the bypass path, which is otherwise not exercised anywhere.

export XrdSecPROTOCOL=host

# shellcheck source=/dev/null
source "$(dirname "${BASH_SOURCE[0]}")/cachelib.sh"

PFC_BLOCKSIZE=$((128 * 1024))
PFC_GSTREAM_PORT=7105
PFC_NBLOCKS=8

function setup_cacheblacklist() {
	cache_setup

	# The blacklist path is baked into cacheblacklist.cfg. Entries are fnmatch
	# patterns against the LFN.
	cat > "${PWD}/${NAME}/blacklist.txt" <<-LIST
	/nocache/*
	LIST
}

function test_cacheblacklist() {
	ensure_gstream_collector

	local size=$((PFC_BLOCKSIZE * PFC_NBLOCKS))

	#---------------------------------------------------------------------------
	echo "--- a blacklisted file is served correctly but never cached"
	#---------------------------------------------------------------------------

	local blocked=nocache/blocked.dat

	assert xrdfs "${ORIGIN_HOST}" mkdir -p /nocache
	make_origin_file blocked.dat "${blocked}" "${PFC_NBLOCKS}"

	assert xrdcp -f "${HOST}//${blocked}" blocked-out.dat
	assert cmp blocked.dat blocked-out.dat

	# Read it twice: a cache that quietly stored it the first time would show
	# up as a changed byte count on the second read.
	assert xrdcp -f "${HOST}//${blocked}" blocked-out2.dat
	assert cmp blocked.dat blocked-out2.dat

	if [[ -e "$(cinfo_path ${blocked})" ]] || [[ -e "$(cache_store)/${blocked}" ]]; then
		error "a blacklisted file was written into the cache"
	fi

	#---------------------------------------------------------------------------
	echo "--- a file outside the blacklist is cached as usual"
	#---------------------------------------------------------------------------

	# Guards against the plugin being wired up as a blanket refusal. Doing it
	# before checking the g-stream also gives the absence check below
	# something to synchronise on: once this file's record has been seen, the
	# stream has been flushed past the reads above.
	local allowed=pfc-allowed.dat
	local acinfo
	acinfo="$(cinfo_path ${allowed})"

	make_origin_file allowed.dat "${allowed}" "${PFC_NBLOCKS}"

	assert xrdcp -f "${HOST}//${allowed}" allowed-out.dat
	assert cmp allowed.dat allowed-out.dat

	assert_fetched "${allowed}" 1 "${size}" \
		"a file outside the blacklist should be fetched and cached"

	#---------------------------------------------------------------------------
	echo "--- the blacklisted file never became a cache file at all"
	#---------------------------------------------------------------------------

	# A refused decision makes Cache::Attach hand back the upstream IO
	# unchanged, so no File object is ever constructed and there is nothing to
	# report on close. The missing record is the positive evidence that the
	# read was proxied rather than cached and then hidden.
	assert_no_gstream_close "${blocked}"

	wait_for_access_record "${acinfo}" 1
	assert_eq "${PFC_NBLOCKS} ${PFC_NBLOCKS} complete" "$(cinfo_blocks "${acinfo}")" \
		"a file outside the blacklist should be complete in the cache"

	assert xrdcp -f "${HOST}//${allowed}" allowed-out2.dat
	assert cmp allowed.dat allowed-out2.dat
	assert_fetched "${allowed}" 2 0 "the second read of an allowed file should be served from disk"
}
