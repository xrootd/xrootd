#!/usr/bin/env bash

export XrdSecPROTOCOL=sss
export XrdSecSSSKT="diglib.keytab"

function setup_diglib() {
	require_commands cut xargs

	assert xrdsssadmin add "${XrdSecSSSKT}" <<< y
	assert xrdsssadmin -u "$(id -un)" -g "$(id -gn)" add "${XrdSecSSSKT}"
	assert xrdsssadmin list "${XrdSecSSSKT}"

	echo "all -core allow sss n=$(id -un)" >| diglib.conf
}

function teardown_diglib() {
	rm "${XrdSecSSSKT}" diglib.conf
}

function test_diglib() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	assert xrdfs "${HOST}" ls /=/

	echo "Show the configuration"
	assert xrdfs "${HOST}" ls /=/conf/etc
	assert xrdfs "${HOST}" cat /=/conf/xrootd.cf
	echo

	echo "Check that listing the core directory fails, since we disallowed it"
	assert_failure xrdfs "${HOST}" ls /=/core

	echo "Check that extra added configuration file is available"
	assert xrdfs "${HOST}" cat /=/conf/etc/diglib.conf
	echo

	# /proc not available on macOS, stop here
	test "$(uname -s)" = "Linux" || exit 0

	echo "Show /proc contents for XRootD process"
	assert xrdfs "${HOST}" ls /=/proc/
	assert xrdfs "${HOST}" ls /=/proc/xrootd/
	echo

	echo "Show XRootD process limits"
	assert xrdfs "${HOST}" cat /=/proc/xrootd/limits
	echo

	EXPECTED="conf logs proc"
	AVAILABLE=$(xrdfs "${HOST}" ls /=/ | cut -d/ -f3 | xargs)
	assert test "${AVAILABLE}" = "${EXPECTED}"
}
