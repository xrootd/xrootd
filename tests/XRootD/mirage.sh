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

	assert xrdcp -f "${SOURCE_DIR}"/mirage.cfg "${HOST}//mirage.cfg"

	# Local and remote size match
	local_size=$(cat "${SOURCE_DIR}"/mirage.cfg | wc -c | tr -d ' ')
	remote_size=$(xrdcp -f "${HOST}"//mirage.cfg - | wc -c | tr -d ' ')
	assert [ "$local_size" = "$remote_size" ]

	# Stat returns correct file size
	local_size=$(cat "${SOURCE_DIR}"/mirage.cfg | wc -c | tr -d ' ')
	remote_size=$(xrdfs "${HOST}"/ stat /mirage.cfg | grep -i size | awk '{print $2}' | tr -d ' ')
	assert [ "$local_size" = "$remote_size" ]

	# Truncate file to 1000 bytes
	assert xrdfs "${HOST}"/ truncate /mirage.cfg 1000
	remote_size=$(xrdfs "${HOST}"/ stat /mirage.cfg | grep -i size | awk '{print $2}')
	assert [ "$remote_size" -eq 1000 ]

	# Single char pattern
	assert xrdfs "${HOST}"/ xattr /mirage.cfg set pattern='a'
	occurrences=$(xrdcp -f "${HOST}"//mirage.cfg - | grep -o a | wc -l)
	assert [ "$occurrences" -eq 1000 ]

	# String pattern
	assert xrdfs "${HOST}"/ xattr /mirage.cfg set pattern='abcde'
	occurrences=$(xrdcp -f "${HOST}"//mirage.cfg - | grep -o abcde | wc -l)
	assert [ "$occurrences" -eq 200 ]

	# Open return code
	assert xrdfs "${HOST}"/ xattr /mirage.cfg set open.return_code=12
	output=$(xrdcp -f "${HOST}"//mirage.cfg - 2>&1)
	[[ "$output" == *"cannot allocate memory"* ]]
	assert xrdfs "${HOST}"/ xattr /mirage.cfg del open.return_code

	# Read return code
	assert xrdfs "${HOST}"/ xattr /mirage.cfg set read.return_code=12
	output=$(xrdcp -f "${HOST}"//mirage.cfg - 2>&1)
	[[ "$output" == *"cannot allocate memory"* ]]
	assert xrdfs "${HOST}"/ xattr /mirage.cfg del read.return_code
}
