#!/usr/bin/env bash

export XrdSecPROTOCOL=host

function setup_host() {
	require_commands diff
}

function test_host() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	assert xrdcp -f "${SOURCE_DIR}"/host.cfg "${HOST}/host.cfg"
	assert xrdcp -f "${HOST}"/host.cfg .
	assert xrdfs "${HOST}" cat /host.cfg
	assert xrdfs "${HOST}" rm  /host.cfg
	assert diff -u "${SOURCE_DIR}"/host.cfg host.cfg
}
