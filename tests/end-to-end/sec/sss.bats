#!/usr/bin/env bash

# The sss security protocol, with a shared secret in a keytab. The tests share
# one server, which accepts only sss, and the keytab of the file.

load ../setup/server.bash

export XrdSecPROTOCOL=sss
export XrdSecSSSKT="$BATS_FILE_TMPDIR/sss.keytab"

setup_file() {
    # a default key, a key for a user to delete, and a key for this user
    xrdsssadmin add "$XrdSecSSSKT" <<< y > /dev/null
    xrdsssadmin -u xrootd -g xrootd add "$XrdSecSSSKT" > /dev/null
    xrdsssadmin -u "$(id -un)" -g "$(id -gn)" add "$XrdSecSSSKT" > /dev/null
    xrdsssadmin -u xrootd -g xrootd del "$XrdSecSSSKT" > /dev/null

    start_server server server sss.cfg
}

teardown_file() {
    stop_servers
}

# Write a keytab of one line, which only the owner can read
write_keytab() {
    ( umask 077 && echo "$1" > bad.keytab )
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    FILE="//t${BATS_TEST_NUMBER}"
    echo 'sss protocol' > local.txt
}

@test "xrdsssadmin list shows the key of the user" {
    run -0 xrdsssadmin list "$XrdSecSSSKT"
    assert_output --partial " $(id -un) "
}

@test "xrdsssadmin list shows each key of a keytab of three keys in the order of their numbers" {
    ( umask 077
      xrdsssadmin -u user1 -g group1 add three.keytab <<< y
      xrdsssadmin -u user3 -g group3 add three.keytab <<< y
      xrdsssadmin -u user2 -g group2 add three.keytab <<< y ) > /dev/null
    run -0 xrdsssadmin list three.keytab
    assert_output --regexp ' 1 .* user1 .* 2 .* user3 .* 3 .* user2 '
}

@test "xrdsssadmin del removes the key of a user" {
    run -0 xrdsssadmin list "$XrdSecSSSKT"
    refute_output --partial ' xrootd '
}

@test "the client authenticates with sss" {
    xrdfs "$SERVER_URL" query config version
    run -0 grep -F 'Authenticated with sss' "$XRD_LOGFILE"
}

@test "an upload with sss authentication succeeds" {
    run -0 xrdcp local.txt "$SERVER_URL/$FILE"
}

@test "a download with sss authentication returns the uploaded file" {
    xrdcp local.txt "$SERVER_URL/$FILE"
    xrdcp "$SERVER_URL/$FILE" copy.txt
    run -0 cmp local.txt copy.txt
}

@test "a keytab which others can read is refused" {
    cp "$XrdSecSSSKT" open.keytab
    chmod 644 open.keytab
    XrdSecSSSKT=open.keytab run ! xrdfs "$SERVER_URL" ls /
}

@test "a keytab which others can read makes the authentication fail" {
    cp "$XrdSecSSSKT" open.keytab
    chmod 644 open.keytab
    XrdSecSSSKT=open.keytab xrdfs "$SERVER_URL" ls / || true
    run -0 grep -F 'Auth failed' "$XRD_LOGFILE"
}

@test "an empty keytab makes the authentication fail" {
    XrdSecSSSKT=/dev/null xrdfs "$SERVER_URL" ls / || true
    run -0 grep -F 'Auth failed' "$XRD_LOGFILE"
}

@test "xrdsssadmin list reports a key without a value" {
    write_keytab '0 u:user1 g:group1 n:name1 N:1 c:1 e:0 f:0 k:'
    run xrdsssadmin list bad.keytab
    assert_output --partial 'keyval has invalid value'
}

@test "xrdsssadmin list reports a key of one digit" {
    write_keytab '0 u:user2 g:group2 n:name2 N:2 c:2 e:0 f:0 k:1'
    run xrdsssadmin list bad.keytab
    assert_output --partial 'keyval has invalid value'
}

@test "xrdsssadmin list reports a key which is not hexadecimal" {
    write_keytab '0 u:user3 g:group3 n:name3 N:3 c:3 e:0 f:0 k:x'
    run xrdsssadmin list bad.keytab
    assert_output --partial 'keyval has invalid value'
}

@test "xrdsssadmin list reports a key of an odd number of digits" {
    write_keytab '0 u:user4 g:group4 n:name4 N:4 c:4 e:0 f:0 k:123456789'
    run xrdsssadmin list bad.keytab
    assert_output --partial 'keyval has invalid value'
}
