#!/usr/bin/env bash

export XrdSecPROTOCOL=krb5

export KRB5CCNAME=${BINARY_DIR}/tests/krb5/krb5cc
export KRB5_CONFIG=${BINARY_DIR}/tests/krb5/krb5.conf

function setup_krb5() {
	require_commands kinit
	assert kinit -p xrootd@XROOTD.ORG <<< xrootd
	assert klist -e
}

function test_krb5() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	assert grep -cq "Authenticated with krb5" "${XRD_LOGFILE}"

	assert xrdcp -f "${SOURCE_DIR}"/krb5.cfg "${HOST}/krb5.cfg"
	assert xrdcp -f "${HOST}"/krb5.cfg .
	assert xrdfs "${HOST}" cat /krb5.cfg
	assert xrdfs "${HOST}" rm  /krb5.cfg
	assert diff -u "${SOURCE_DIR}"/krb5.cfg krb5.cfg
}

