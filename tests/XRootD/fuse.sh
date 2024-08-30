#!/usr/bin/env bash

export FUSE_DIR="${LOCAL_DIR}"
export XROOTDFS_NO_ALLOW_OTHER=1

function setup_fuse() {
	require_commands mount umount openssl xrootdfs
	test -w /dev/fuse || error "FUSE device not available (/dev/fuse)"
}

function mount_fuse() {
	# Note: xrootdfs needs two slashes in the URL to mount correctly
	if ! xrootdfs -o rdr="${HOST}/" "${FUSE_DIR}"; then
		error "failed to mount XRootD FUSE mount at ${FUSE_DIR}"
	fi

	# Make sure we cleanup the mount upon exit after we've mounted it
	trap unmount_fuse ERR INT TERM QUIT ABRT EXIT
}

function unmount_fuse() {
	STATUS=$?
	dirs -c
	cd "${BINARY_DIR}" || exit 1
	[[ -d "${FUSE_DIR}" ]] || return
	if ! umount "${FUSE_DIR}"; then
		error "failed to unmount XRootD FUSE mount at ${FUSE_DIR}"
	fi
	rmdir "${FUSE_DIR}"
	exit "${STATUS}"
}

function test_fuse() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	mount_fuse
	mount -t fuse || error "cannot list FUSE mounts"
	pushd "${FUSE_DIR}" || error "could not change into FUSE directory"

	FILES=$(seq -w 1 "${NFILES:-10}")

	for i in ${FILES}; do
		assert openssl rand -base64 -out "${i}.ref" $((1024 * (RANDOM % 1024)))

		# TODO: Fix "Resource deadlock avoided" if this sleep is removed
		sleep 0.1

		assert cp "${i}.ref" "${i}.dat"

		REF32C=$(xrdcrc32c < "${i}.ref" | cut -d' '  -f1)
		NEW32C=$(xrdcrc32c < "${i}.dat" | cut -d' '  -f1)

		REFA32=$(xrdadler32 < "${i}.ref" | cut -d' '  -f1)
		NEWA32=$(xrdadler32 < "${i}.dat" | cut -d' '  -f1)

		if [[ "${NEWA32}" != "${REFA32}" ]]; then
			echo 1>&2 "${i}: adler32: reference: ${REFA32}, copy: ${NEWA32}"
			error "adler32 checksum check failed for file: ${i}.dat"
		fi

		if [[ "${NEW32C}" != "${REF32C}" ]]; then
			echo 1>&2 "${i}:  crc32c: reference: ${REF32C}, copy: ${NEW32C}"
			error "crc32 checksum check failed for file: ${i}.dat"
		fi
	done

	assert ls -l

	for i in ${FILES}; do
		assert mv "${i}.dat" "${i}.raw"
		assert mv "${i}.raw" "${i}.ref"
		assert rm "${i}.ref"
	done

	assert mkdir tmp

	# create many directories and files, then remove them recursively

	for dir in $(seq -w 1 64); do
		TMPDIR="tmp/test-${dir}/a/b/c"
		assert mkdir -p "${TMPDIR}"
		for file in $(seq -w 1 64); do
			assert openssl rand -base64 -out "${TMPDIR}/${file}.raw" 8192
		done
	done

	assert rm -rf tmp/*
	assert rmdir tmp

	popd || exit 1
}
