#!/usr/bin/env bash

export XrdSecPROTOCOL=sss
export XrdSecSSSKT="sss.keytab"

function setup_sss() {
	require_commands diff grep truncate

	assert xrdsssadmin add "${XrdSecSSSKT}" <<< y
	assert xrdsssadmin -u xrootd -g xrootd add "${XrdSecSSSKT}"
	assert xrdsssadmin -u "$(id -un)" -g "$(id -gn)" add "${XrdSecSSSKT}"
	assert xrdsssadmin list "${XrdSecSSSKT}"
	assert xrdsssadmin -u xrootd -g xrootd del "${XrdSecSSSKT}"
}

function teardown_sss() {
	rm ${XrdSecSSSKT}
}

function test_sss() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	# Make sure we did authenticate with sss
	assert grep -c "Authenticated with sss" "${XRD_LOGFILE}"

	# Copy a file to the server and back
	assert xrdcp -f "${SOURCE_DIR}"/sss.cfg "${HOST}/sss.cfg"
	assert xrdcp -f "${HOST}"/sss.cfg .

	# Show the configuration
	assert xrdfs "${HOST}" cat /sss.cfg
	assert xrdfs "${HOST}" rm  /sss.cfg

	# Check against original file
	assert diff -u "${SOURCE_DIR}"/sss.cfg sss.cfg

	assert truncate -s 0 "${XRD_LOGFILE}"

	# Make sure auth fails with a bad (insecure) sss keytab
	chmod 644 "${XrdSecSSSKT}"
	assert_failure xrdfs "${HOST}" ls /
	assert grep "Auth failed" "${XRD_LOGFILE}"

	assert truncate -s 0 "${XRD_LOGFILE}"

	# Make sure auth fails with an invalid sss keytab (/dev/null)
	assert_failure env XrdSecSSSKT=/dev/null xrdfs "${HOST}" ls /
	assert grep "Auth failed" "${XRD_LOGFILE}"
}
