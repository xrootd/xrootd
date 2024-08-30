#!/usr/bin/env bash

function setup_http() {
	require_commands davix-{get,put,mkdir,rm} openssl
	openssl rand -base64 -out macaroons-secret 64
}

function teardown_http() {
	rm macaroons-secret
}

function test_http() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	# create local temporary directory
	TMPDIR=$(mktemp -d "${PWD}/${NAME}/test-XXXXXX")

	# create remote temporary directory
	# this will get cleaned up by CMake upon fixture tear down
	assert xrdfs "${HOST}" mkdir -p "${TMPDIR}"

	# from now on, we use HTTP
	export HOST=http://localhost:8094

	# create local files with random contents using OpenSSL

	FILES=$(seq -w 1 "${NFILES:-10}")

	for i in $FILES; do
		assert openssl rand -base64 -out "${TMPDIR}/${i}.ref" $((1024 * (RANDOM + 1)))
	done

	# upload local files to the server in parallel with davix-put

	for i in $FILES; do
		assert davix-put "${TMPDIR}/${i}.ref" "${HOST}/${TMPDIR}/${i}.ref"
	done

	# list uploaded files, then download them to check for corruption

	assert davix-ls "${HOST}/${TMPDIR}"

	# download files back with davix-get

	for i in $FILES; do
		assert davix-get "${HOST}/${TMPDIR}/${i}.ref" "${TMPDIR}/${i}.dat"
	done

	# check that all checksums for downloaded files match

	for i in $FILES; do
		REF32C=$(xrdcrc32c < "${TMPDIR}/${i}.ref" | cut -d' '  -f1)
		NEW32C=$(xrdcrc32c < "${TMPDIR}/${i}.dat" | cut -d' '  -f1)

		REFA32=$(xrdadler32 < "${TMPDIR}/${i}.ref" | cut -d' '  -f1)
		NEWA32=$(xrdadler32 < "${TMPDIR}/${i}.dat" | cut -d' '  -f1)

		if [[ "${NEWA32}" != "${REFA32}" ]]; then
			echo 1>&2 "${i}: adler32: reference: ${REFA32}, downloaded: ${NEWA32}"
			error "adler32 checksum check failed for file: ${i}.dat"
		fi

		if [[ "${NEW32C}" != "${REF32C}" ]]; then
			echo 1>&2 "${i}:  crc32c: reference: ${REF32C}, downloaded: ${NEW32C}"
			error "crc32 checksum check failed for file: ${i}.dat"
		fi
	done

	assert davix-ls "${HOST}/"

	for i in $FILES; do
	       assert davix-rm "${HOST}/${TMPDIR}/${i}.ref"
	done

	wait
}
