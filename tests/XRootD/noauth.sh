#!/usr/bin/env bash

function setup_noauth() {
	require_commands openssl
}

function test_noauth() {
	echo
	echo "client: XRootD $(assert xrdcp --version 2>&1)"
	echo "server: XRootD $(assert xrdfs "${HOST}" query config version 2>&1)"
	echo
	openssl version
	echo

	CONFIG_PARAMS=(
		sitename
		role
		bind_max
		chksum
		pio_max
		readv_ior_max
		readv_iov_max
		tpc
		wan_port
		wan_window
		window
		cms
	)

	for PARAM in "${CONFIG_PARAMS[@]}"; do
	       echo -e "${PARAM} = $(assert xrdfs "${HOST}" query config "${PARAM}" 2>&1)"
	done

	# some extra query commands that don't make any changes

	assert xrdfs "${HOST}" stat /
	assert xrdfs "${HOST}" statvfs /
	assert xrdfs "${HOST}" spaceinfo /

	# create local temporary directory
	TMPDIR=$(mktemp -d "${PWD}/${NAME}/test-XXXXXX")

	# create remote temporary directory
	# this will get cleaned up by CMake upon fixture tear down
	assert xrdfs "${HOST}" mkdir -p "${TMPDIR}"

	# create local files with random contents using OpenSSL

	FILES=$(seq -w 1 "${NFILES:-10}")

	for i in $FILES; do
		assert openssl rand -base64 -out "${TMPDIR}/${i}.ref" $((1024 * (RANDOM + 1)))
	done

	# upload local files to the server in parallel

	for i in $FILES; do
		assert xrdcp -np "${TMPDIR}/${i}.ref" "${HOST}/${TMPDIR}/${i}.ref"
	done

	# list uploaded files, then download them to check for corruption

	assert xrdfs "${HOST}" ls -l "${TMPDIR}"

	for i in $FILES; do
		assert xrdcp -np "${HOST}/${TMPDIR}/${i}.ref" "${TMPDIR}/${i}.dat"
	done

	# check that all checksums for downloaded files match

	for i in $FILES; do
	    REF32C=$(xrdcrc32c < "${TMPDIR}/${i}.ref" | cut -d' '  -f1)
	    NEW32C=$(xrdcrc32c < "${TMPDIR}/${i}.dat" | cut -d' '  -f1)

	    REFA32=$(xrdadler32 < "${TMPDIR}/${i}.ref" | cut -d' '  -f1)
	    NEWA32=$(xrdadler32 < "${TMPDIR}/${i}.dat" | cut -d' '  -f1)

	    if setfattr -n user.checksum -v "${REF32C}" "${TMPDIR}/${i}.ref"; then
		SRV32C=$(xrdfs "${HOST}" query checksum "${TMPDIR}/${i}.ref?cks.type=crc32c" | cut -d' ' -f2)
		SRVA32=$(xrdfs "${HOST}" query checksum "${TMPDIR}/${i}.ref?cks.type=adler32" | cut -d' ' -f2)
	    else
		echo "Extended attributes not supported, using downloaded checksums for server checks"
		SRV32C=${NEW32C} SRVA32=${NEWA32} # use downloaded file checksum if xattr not supported
	    fi

	    if [[ "${NEWA32}" != "${REFA32}" || "${SRVA32}" != "${REFA32}" ]]; then
		echo 1>&2 "${i}: adler32: reference: ${REFA32}, server: ${SRVA32}, downloaded: ${NEWA32}"
		error "adler32 checksum check failed for file: ${i}.dat"
	    fi

	    if [[ "${NEW32C}" != "${REF32C}" || "${SRV32C}" != "${REF32C}" ]]; then
		echo 1>&2 "${i}:  crc32c: reference: ${REF32C}, server: ${SRV32C}, downloaded: ${NEW32C}"
		error "crc32 checksum check failed for file: ${i}.dat"
	    fi
	done

	assert xrdfs "${HOST}" ls -R /

	for i in $FILES; do
	       FILE="${TMPDIR}/${i}.ref"

	       assert xrdfs "${HOST}" stat "${FILE}"
	       assert xrdfs "${HOST}" xattr "${FILE}" list

	       assert xrdfs "${HOST}" chmod "${FILE}" rwxr-xr-x
	       assert xrdfs "${HOST}" chmod "${FILE}" rwxrwxr-x
	       assert xrdfs "${HOST}" chmod "${FILE}" rw-r--r--

	       assert xrdfs "${HOST}" ls "${FILE}"
	       assert xrdfs "${HOST}" ls -u "${FILE}"
	       assert xrdfs "${HOST}" ls -l "${FILE}"
	       assert xrdfs "${HOST}" ls -D "${FILE}"

	       assert xrdfs "${HOST}" locate -r "${FILE}"
	       assert xrdfs "${HOST}" locate -n "${FILE}"
	       assert xrdfs "${HOST}" locate -d "${FILE}"
	       assert xrdfs "${HOST}" locate -m "${FILE}"
	       assert xrdfs "${HOST}" locate -i "${FILE}"
	       assert xrdfs "${HOST}" locate -p "${FILE}"

	       assert xrdfs "${HOST}" truncate "${FILE}" 64
	       assert xrdfs "${HOST}" cat "${FILE}"
	       echo
	       assert xrdfs "${HOST}" tail -c 32 "${FILE}"
	       echo
	       assert xrdfs "${HOST}" rm "${FILE}" &
	done

	wait

	# check return code for xrdfs rm
	# create another 6 files, which should be deleted during the test
	for i in $(seq -w 1 6) ; do
		assert openssl rand -out "${TMPDIR}/${i}.exists.ref" $((1024 * (RANDOM + 1)))
		assert xrdcp -np "${TMPDIR}/${i}.exists.ref" "${HOST}/${TMPDIR}/${i}.exists.ref"
	done

	# remove 3 existing, should succeed not error
	assert xrdfs "${HOST}" rm "${TMPDIR}"/{1..3}.exists.ref

	# remove 3 not existing, should error
	assert_failure xrdfs "${HOST}" rm "${TMPDIR}"/not_exists_{1..3}.ref

	# remove 2 existing, 1 not existing should error
	assert_failure xrdfs "${HOST}" rm "${TMPDIR}"/{4,5}.exists.ref "${TMPDIR}"/not_exists_4.ref

	# remove 1 existing, 2 not existing should error
	assert_failure xrdfs "${HOST}" rm "${TMPDIR}"/6.exists.ref "${TMPDIR}"/not_exists_{5,6}.ref

	assert xrdfs "${HOST}" rmdir "${TMPDIR}"
}
