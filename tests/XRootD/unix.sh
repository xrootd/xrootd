#!/usr/bin/env bash

export XrdSecPROTOCOL=unix

function setup_unix() {
	require_commands diff grep
}

function test_unix() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	assert grep "Authenticated with unix" "${XRD_LOGFILE}"
	assert xrdcp -f "${SOURCE_DIR}"/unix.cfg "${HOST}/unix.cfg"
	assert xrdcp -f "${HOST}"/unix.cfg .
	assert xrdfs "${HOST}" cat /unix.cfg
	assert xrdfs "${HOST}" rm  /unix.cfg
	assert diff -u "${SOURCE_DIR}"/unix.cfg unix.cfg
}
