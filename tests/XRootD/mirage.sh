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
	assert xrdcp -f "${ORIGIN_HOST}"//mirage.cfg mirage.origin.cfg
	assert diff -u "${SOURCE_DIR}"/mirage.cfg mirage.origin.cfg

	assert xrdcp -f "${HOST}"//mirage.cfg host.mirage.cfg
	assert diff -u "${SOURCE_DIR}"/mirage.cfg host.mirage.cfg
}
