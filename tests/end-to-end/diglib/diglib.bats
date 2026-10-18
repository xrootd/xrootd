#!/usr/bin/env bash

# The digital library: the paths /=/ of the server, which show its
# configuration, logs and process information. The tests share one server.

load ../setup/server.bash

export XrdSecPROTOCOL=sss
export XrdSecSSSKT="$BATS_FILE_TMPDIR/diglib.keytab"

setup_file() {
    xrdsssadmin add "$XrdSecSSSKT" <<< y > /dev/null
    xrdsssadmin -u "$(id -un)" -g "$(id -gn)" add "$XrdSecSSSKT" > /dev/null

    echo "all -core allow sss n=$(id -un)" > "$BATS_FILE_TMPDIR/diglib.conf"

    start_server server server diglib.cfg
}

teardown_file() {
    stop_servers
}

@test "the root of the digital library has conf, logs and proc" {
    [[ -d /proc ]] || skip 'no /proc'
    run -0 bats_pipe xrdfs "$SERVER_URL" ls /=/ \| cut -d/ -f3 \| xargs
    assert_output 'conf logs proc'
}

@test "the root of the digital library has conf and logs without /proc" {
    [[ ! -d /proc ]] || skip 'the system has /proc'
    run -0 bats_pipe xrdfs "$SERVER_URL" ls /=/ \| cut -d/ -f3 \| xargs
    assert_output 'conf logs'
}

@test "the digital library lists the configuration files" {
    run -0 xrdfs "$SERVER_URL" ls /=/conf/etc
}

@test "the digital library shows the configuration of the server" {
    run -0 xrdfs "$SERVER_URL" cat /=/conf/xrootd.cf
    assert_output --partial 'xrootd.diglib'
}

@test "the digital library refuses the core directory, which the configuration denies" {
    run ! xrdfs "$SERVER_URL" ls /=/core
}

@test "the digital library shows an added configuration file" {
    run -0 xrdfs "$SERVER_URL" cat /=/conf/etc/diglib.conf
    assert_output "all -core allow sss n=$(id -un)"
}

@test "the digital library lists the processes" {
    [[ -d /proc ]] || skip 'no /proc'
    run -0 xrdfs "$SERVER_URL" ls /=/proc/
}

@test "the digital library lists the xrootd process" {
    [[ -d /proc ]] || skip 'no /proc'
    run -0 xrdfs "$SERVER_URL" ls /=/proc/xrootd/
}

@test "the digital library shows the limits of the xrootd process" {
    [[ -d /proc ]] || skip 'no /proc'
    run -0 xrdfs "$SERVER_URL" cat /=/proc/xrootd/limits
    assert_output --partial 'Max open files'
}
