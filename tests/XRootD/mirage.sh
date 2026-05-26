#!/usr/bin/env bash

export XrdSecPROTOCOL=host

function setup_mirage() {
	require_commands diff
}

function test_mirage() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	ORIGIN_HOST="root://localhost:6543"

	assert xrdcp -f "${SOURCE_DIR}"/mirage.cfg "${ORIGIN_HOST}//mirage.cfg"

	# Local and remote size match
	local_size=$(stat -c '%s' "${SOURCE_DIR}"/mirage.cfg)
	remote_size=$(xrdcp -f "${ORIGIN_HOST}"//mirage.cfg - | wc -c)
	assert [ "$local_size" = "$remote_size" ]

	# Stat returns correct file size
	local_size=$(stat -c '%s' "${SOURCE_DIR}"/mirage.cfg)
	remote_size=$(xrdfs "${ORIGIN_HOST}"/ stat /mirage.cfg | grep -i size | awk '{print $2}')
	assert [ "$local_size" = "$remote_size" ]

	# Truncate file to 1000 bytes
	assert xrdfs "${ORIGIN_HOST}"/ truncate /mirage.cfg 1000
	remote_size=$(xrdfs "${ORIGIN_HOST}"/ stat /mirage.cfg | grep -i size | awk '{print $2}')
	assert [ "$remote_size" -eq 1000 ]

	# Single char pattern
	assert xrdfs "${ORIGIN_HOST}"/ xattr /mirage.cfg set pattern='a'
	occurrences=$(xrdcp -f "${ORIGIN_HOST}"//mirage.cfg - | grep -o a | wc -l)
	assert [ "$occurrences" -eq 1000 ]

	# String pattern
	assert xrdfs "${ORIGIN_HOST}"/ xattr /mirage.cfg set pattern='abcde'
	occurrences=$(xrdcp -f "${ORIGIN_HOST}"//mirage.cfg - | grep -o abcde | wc -l)
	assert [ "$occurrences" -eq 200 ]

	# Open return code
	assert xrdfs "${ORIGIN_HOST}"/ xattr /mirage.cfg set open.return_code=12
	output=$(xrdcp -f "${ORIGIN_HOST}"//mirage.cfg - 2>&1)
	[[ "$output" == *"cannot allocate memory"* ]]
	assert xrdfs "${ORIGIN_HOST}"/ xattr /mirage.cfg del open.return_code

	# Read return code
	assert xrdfs "${ORIGIN_HOST}"/ xattr /mirage.cfg set read.return_code=12
	output=$(xrdcp -f "${ORIGIN_HOST}"//mirage.cfg - 2>&1)
	[[ "$output" == *"cannot allocate memory"* ]]
	assert xrdfs "${ORIGIN_HOST}"/ xattr /mirage.cfg del read.return_code
}
