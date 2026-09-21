#!/usr/bin/env bash

export XrdSecPROTOCOL=host

function setup_cache() {
	require_commands diff
}

function test_cache() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	ORIGIN_HOST="root://localhost:5094"

	assert xrdcp -f "${SOURCE_DIR}"/cache.cfg "${ORIGIN_HOST}//cache.cfg"
	assert xrdcp -f "${ORIGIN_HOST}"//cache.cfg cache.origin.cfg
	assert diff -u "${SOURCE_DIR}"/cache.cfg cache.origin.cfg

	assert xrdcp -f "${HOST}"//cache.cfg host.cache.cfg
	assert diff -u "${SOURCE_DIR}"/cache.cfg host.cache.cfg
}
