#!/usr/bin/env bash

# A Kerberos key distribution center (KDC) for the tests of the krb5 security
# protocol. A test file loads this file after server.bash, and calls start_kdc
# before it starts a server with the snippet krb5; stop_servers stops the KDC.

# Programs of the KDC are often in /usr/sbin
PATH="$PATH:/usr/sbin"

readonly KDC_REALM=XROOTD.ORG

# start_kdc
#
# Start a KDC for the realm XROOTD.ORG in the directory "kdc" of the current
# context, on a free port, with the principals host/localhost, whose key is in
# $KDC_DIR/krb5.keytab, and xrootd, with the password xrootd. Export the
# environment of the Kerberos library for the client and the server.
start_kdc() {
    require_commands krb5kdc kdb5_util kadmin.local kinit python3

    local dir port
    dir="$(_setup_tmpdir)/kdc"
    port=$(free_port)
    mkdir -p "$dir"

    cat > "$dir/kdc.conf" <<EOF
[kdcdefaults]
kdc_listen = $port
kdc_tcp_listen = $port

[realms]
$KDC_REALM = {
  database_name = $dir/db
  key_stash_file = $dir/stash
  acl_file = ""
  max_life = 1h 0m 0s
}

[logging]
kdc = FILE:$dir/kdc.log
default = FILE:$dir/krb5lib.log
EOF

    cat > "$dir/krb5.conf" <<EOF
[libdefaults]
default_realm = $KDC_REALM
dns_canonicalize_hostname = false
rdns = false
dns_lookup_realm = false
dns_lookup_kdc = false
default_ccache_name = FILE:$dir/krb5cc

[domain_realm]
localhost = $KDC_REALM

[realms]
$KDC_REALM = {
  kdc = localhost:$port
}
EOF

    export KRB5_CONFIG="$dir/krb5.conf"
    export KRB5_KDC_PROFILE="$dir/kdc.conf"
    export KRB5CCNAME="FILE:$dir/krb5cc"
    export KRB5RCACHEDIR="$dir"
    export KDC_DIR="$dir"

    kdb5_util -r "$KDC_REALM" -d "$dir/db" -P xrootd create -s > "$dir/create.log" 2>&1 || return 1
    # the KDC must not keep the descriptors 3 and 4, on which BATS reads
    krb5kdc -r "$KDC_REALM" -d "$dir/db" -P "$dir/kdc.pid" 3>&- 4>&- || return 1

    kadmin.local -r "$KDC_REALM" -d "$dir/db" > "$dir/kadmin.log" 2>&1 <<EOF || return 1
add_principal -randkey host/localhost@$KDC_REALM
ktadd -k $dir/krb5.keytab host/localhost@$KDC_REALM
add_principal -pw xrootd xrootd@$KDC_REALM
EOF

    # the KDC writes its pid file once it serves requests
    for _ in {1..100}; do
        [[ -s "$dir/kdc.pid" ]] && return 0
        sleep 0.05
    done

    echo "start_kdc: the KDC did not start" >&2
    cat "$dir"/*.log >&2
    return 1
}
