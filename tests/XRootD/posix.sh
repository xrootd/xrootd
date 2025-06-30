#!/usr/bin/env bash

function setup_posix() {
	require_commands cc

	# Create some sample text files
	echo "This is a local text file." >| "${LOCAL_DIR}"/local.txt
	echo "This is a remote text file." >| "${REMOTE_DIR}"/remote.txt
}

function test_posix() {
	# XrdPosix is not yet supported with musl libc
	cc -dumpmachine | grep -q musl && return

	# Make sure all symbols are actually resolved correctly
	MISSING_SYMBOLS=$(env XRDPOSIX_REPORT=1 xrdposix-cat /dev/null 2>&1)

        if grep -q 'Unable to resolve' <<< "${MISSING_SYMBOLS}"; then
                echo "XrdPosix cannot resolve some symbols:"
                echo "${MISSING_SYMBOLS}"
                exit 1
        fi

	# Check that cat with the local files still works

	assert xrdposix-cat "${LOCAL_DIR}"/local.txt
	assert xrdposix-cat "${REMOTE_DIR}"/remote.txt

	assert_failure xrdposix-cat /this/does/not/exist.txt

	# Check that cat with remote file URL works

	assert xrdposix-cat "${HOST}"/remote.txt

	assert_failure xrdposix-cat "${HOST}"/does/not/exist.txt

	# Use XrdPosix via virtual mount point

	export XROOTD_VMP="${HOST#root://}:/xrootd/=/"

	assert xrdposix-cat /xrootd/remote.txt
}
