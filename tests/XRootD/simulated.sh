#!/usr/bin/env bash

export XrdSecPROTOCOL=host

function setup_simulated() {
	require_commands diff
}

function test_simulated() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	ORIGIN_HOST="root://localhost:6543"

	assert xrdcp -f "${SOURCE_DIR}"/simulated.cfg "${ORIGIN_HOST}//simulated.cfg"
	assert xrdcp -f "${ORIGIN_HOST}"//simulated.cfg simulated.origin.cfg
	assert diff -u "${SOURCE_DIR}"/simulated.cfg simulated.origin.cfg

	assert xrdcp -f "${HOST}"//simulated.cfg host.simulated.cfg
	assert diff -u "${SOURCE_DIR}"/simulated.cfg host.simulated.cfg
}
