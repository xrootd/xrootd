#!/usr/bin/env bash
#
# pfc.spaces: data files and their cinfo metadata go to two different OSS
# spaces, so a deployment can put the metadata on faster storage. Both halves
# have to end up in the right place and the file still has to read back.

export XrdSecPROTOCOL=host

# shellcheck source=/dev/null
source "$(dirname "${BASH_SOURCE[0]}")/cachelib.sh"

PFC_BLOCKSIZE=$((128 * 1024))
PFC_GSTREAM_PORT=7107
PFC_NBLOCKS=16

function space_dir() {
	echo "${PWD}/${NAME}/space-$1"
}

function setup_cachespaces() {
	cache_setup

	# Paths must match the oss.space lines in cachespaces.cfg, and must exist
	# before the server starts: PFC stats each space at config time and
	# refuses to come up if one has less than 10 MB free.
	mkdir -p "$(space_dir data)" "$(space_dir meta)"
}

function teardown_cachespaces() {
	rm -rf "$(space_dir data)" "$(space_dir meta)"
}

# Literal target of a symlink, not the resolved path: the comparison below is
# against the space directory as spelled in the config.
function link_target() {
	readlink "$1" 2>/dev/null || true
}

function test_cachespaces() {
	ensure_gstream_collector

	local lfn=pfc-spaces.dat
	local size=$((PFC_BLOCKSIZE * PFC_NBLOCKS))
	local data_link meta_link data_target meta_target

	make_origin_file spaces.dat "${lfn}" "${PFC_NBLOCKS}"

	#---------------------------------------------------------------------------
	echo "--- the file reads back correctly with data and meta split"
	#---------------------------------------------------------------------------

	assert xrdcp -f "${HOST}//${lfn}" spaces-out.dat
	assert cmp spaces.dat spaces-out.dat

	assert_fetched "${lfn}" 1 "${size}" "cold read should fetch the whole file"

	#---------------------------------------------------------------------------
	echo "--- the data file lands in the data space, the cinfo in the meta space"
	#---------------------------------------------------------------------------

	# With spaces configured the cache store holds symlinks, and the real
	# files live under the space directories with OSS-assigned names. What
	# matters is which space each symlink points into.

	data_link="$(cache_store)/${lfn}"
	meta_link="$(cinfo_path ${lfn})"

	wait_for_access_record "${meta_link}" 1

	if [[ ! -L "${data_link}" ]]; then
		error "expected ${data_link} to be a symlink into a space"
	fi
	if [[ ! -L "${meta_link}" ]]; then
		error "expected ${meta_link} to be a symlink into a space"
	fi

	data_target="$(link_target "${data_link}")"
	meta_target="$(link_target "${meta_link}")"

	if [[ "${data_target}" != "$(space_dir data)"/* ]]; then
		error "data file is not in the data space: ${data_target}"
	fi
	if [[ "${meta_target}" != "$(space_dir meta)"/* ]]; then
		error "cinfo is not in the meta space: ${meta_target}"
	fi

	# Not a dangling link, and the real bytes are there.
	if [[ ! -f "${data_target}" ]]; then
		error "data symlink does not resolve to a file: ${data_target}"
	fi
	assert cmp spaces.dat "${data_target}"

	assert_eq "${PFC_NBLOCKS} ${PFC_NBLOCKS} complete" "$(cinfo_blocks "${meta_link}")" \
		"the cinfo in the meta space should show the file complete"

	#---------------------------------------------------------------------------
	echo "--- neither space holds the other one's half"
	#---------------------------------------------------------------------------

	# The whole point of the split, checked by size rather than by name since
	# the OSS chooses the names. Sizes come from wc -c in a loop: find's -size
	# is not dependable across the find implementations in CI.
	local f n_data_sized=0
	while read -r f; do
		[[ -n "${f}" ]] || continue
		[[ "$(file_bytes "${f}")" -eq "${size}" ]] && n_data_sized=$((n_data_sized + 1))
	done < <(find "$(space_dir data)" -type f)

	if [[ "${n_data_sized}" -eq 0 ]]; then
		error "no file of the data file's size under the data space"
	fi

	while read -r f; do
		[[ -n "${f}" ]] || continue
		if [[ "$(file_bytes "${f}")" -eq "${size}" ]]; then
			error "a full-size data file was written into the meta space: ${f}"
		fi
	done < <(find "$(space_dir meta)" -type f)

	#---------------------------------------------------------------------------
	echo "--- and the split copy serves a warm read"
	#---------------------------------------------------------------------------

	assert xrdcp -f "${HOST}//${lfn}" spaces-out2.dat
	assert cmp spaces.dat spaces-out2.dat
	assert_fetched "${lfn}" 2 0 "warm read should be served from the data space"
}
