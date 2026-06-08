#!/usr/bin/env bash

set -e

# Add /usr/sbin to PATH so that kerberos binaries (kdb5_util, krb5kdc
# and kadmin.local) are found when the test is run by a user without
# it.

export PATH="${PATH}:/usr/sbin"

export KRB5CCNAME="${PWD}/krb5cc"
export KRB5_CONFIG="${PWD}/krb5.conf"
export KRB5_KDC_PROFILE="${PWD}/kdc.conf"

function reserve_ports() {
	python3 - <<'PY'
import socket

sockets = []
for _ in range(3):
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	sock.bind(("127.0.0.1", 0))
	sockets.append(sock)

print(*(sock.getsockname()[1] for sock in sockets))
PY
}

function configure_ports() {
	read -r KDC_PORT KADMIN_PORT KPASSWD_PORT <<<"$(reserve_ports)"

	sed -i -E \
		-e "s/(kdc_listen[[:space:]]*=[[:space:]]*)[0-9]+/\\1${KDC_PORT}/" \
		-e "s/(kdc_tcp_listen[[:space:]]*=[[:space:]]*)[0-9]+/\\1${KDC_PORT}/" \
		-e "s/(kdc[[:space:]]*=[[:space:]]*localhost:)[0-9]+/\\1${KDC_PORT}/" \
		-e "s/(master_kdc[[:space:]]*=[[:space:]]*localhost:)[0-9]+/\\1${KDC_PORT}/" \
		-e "s/(kpasswd_server[[:space:]]*=[[:space:]]*localhost:)[0-9]+/\\1${KPASSWD_PORT}/" \
		-e "s/(admin_server[[:space:]]*=[[:space:]]*localhost:)[0-9]+/\\1${KADMIN_PORT}/" \
		"${KRB5_CONFIG}"

	sed -i -E \
		-e "s/(kdc_listen[[:space:]]*=[[:space:]]*)[0-9]+/\\1${KDC_PORT}/" \
		-e "s/(kdc_tcp_listen[[:space:]]*=[[:space:]]*)[0-9]+/\\1${KDC_PORT}/" \
		-e "s/(kadmind_listen[[:space:]]*=[[:space:]]*)[0-9]+/\\1${KADMIN_PORT}/" \
		-e "s/(kpasswd_listen[[:space:]]*=[[:space:]]*)[0-9]+/\\1${KPASSWD_PORT}/" \
		"${KRB5_KDC_PROFILE}"
}

function setup() {
	rm -f "${KRB5CCNAME}" krb5.keytab kdc/{db*,*.{log,pem,srl}}
	configure_ports

	pushd kdc >/dev/null || exit 1

	# Create certificates for KDC PKINIT preauthentication mechanism
	openssl genrsa -out cakey.pem 2048
	openssl req -key cakey.pem -new -x509 -out cacert.pem -days 30 \
	    -subj '/O=XRootD/CN=XRootD Kerberos CA'

	openssl genrsa -out kdckey.pem 2048
	openssl req -new -out kdc.req -key kdckey.pem \
	    -subj '/O=XRootD/CN=XRootD Kerberos Realm'
	env REALM=XROOTD.ORG openssl x509 -req -in kdc.req \
	    -CAkey cakey.pem -CA cacert.pem -out kdc.pem -days 30 \
	    -extfile extensions.kdc -extensions kdc_cert -CAcreateserial
	rm kdc.req

	openssl genrsa -out clientkey.pem 2048
	openssl req -new -key clientkey.pem -out client.req \
	    -subj '/O=XRootD/CN=XRootD Kerberos Client'
	env REALM=XROOTD.ORG CLIENT=xrootd@XROOTD.ORG \
	    openssl x509 -in client.req -CAkey cakey.pem -CA cacert.pem \
	    -req -extensions client_cert -extfile extensions.client \
	    -days 30 -out client.pem
	rm client.req

	popd >/dev/null || exit 1

	# Create the KDC database
	kdb5_util create -s -r XROOTD.ORG -P xrootd

	# Start the KDC daemons
	krb5kdc -P "${PWD}"/krb5kdc.pid

	# Not really needed, since we use kadmin.local
	# kadmind -P ${PWD}/kadmind.pid

	# Add principals for the server and client to KDC database
	kadmin.local -r XROOTD.ORG <<-EOF
	add_principal -randkey -kvno 1 host/localhost@XROOTD.ORG
	ktadd -k krb5.keytab host/localhost
	add_principal xrootd@XROOTD.ORG
	xrootd
	xrootd
	EOF

	# Display KDC database entries
	kdb5_util tabdump -o - keyinfo

	# Display contents of server keytab
	klist -kte krb5.keytab
}

function teardown() {
	export PIDFILE=krb5kdc.pid
	if test -s "${PIDFILE}"; then
		PID="$(ps -o pid= "$(cat "${PIDFILE}")" || true)"
		if test -n "${PID}"; then
			kill -s TERM "${PID}"
			rm "${PIDFILE}"
		fi
	fi
	tail -n "${MAXLINES:-50}" kdc/*.log
	rm -f "${KRB5CCNAME}" krb5.keytab kdc/{db*,*.{log,pem,srl}}
}

[[ $(type -t "$1") == "function" ]] || die "unknown command: $1"
"$@"
