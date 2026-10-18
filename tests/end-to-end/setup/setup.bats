#!/usr/bin/env bash

# Tests of the common server setup itself (see server.bash).

load server.bash

setup_file() {
    start_server shared server
    start_server other server
}

teardown_file() {
    stop_servers
}

teardown() {
    stop_servers
}

@test "a server shared by the file answers on its URL" {
    run -0 xrdfs "$SHARED_URL" query config version
}

@test "a server answers on its IPv4 URL" {
    run -0 xrdfs "$SHARED_URL4" query config version
}

@test "a server answers on its IPv6 URL" {
    require_ipv6
    run -0 xrdfs "$SHARED_URL6" query config version
}

@test "two servers of a file use different ports" {
    assert_not_equal "$SHARED_PORT" "$OTHER_PORT"
}

@test "the data directory of a server is its local root" {
    echo 'seed' > "$SHARED_DATA/data-dir"
    run -0 xrdcp "$SHARED_URL//data-dir" -
    assert_output 'seed'
}

@test "the server log has the configuration of the role" {
    run -0 grep -F "oss.localroot $BATS_FILE_TMPDIR/shared/data" "$SHARED_LOG"
}

@test "a server of a test runs below the test directory" {
    start_server mine server
    assert_equal "$MINE_DATA" "$BATS_TEST_TMPDIR/mine/data"
}

@test "a server of a test can use the name of a server of the file" {
    start_server shared server
    run -0 xrdfs "$SHARED_URL" query config version
}

@test "stop_servers stops the servers of the test" {
    local pid
    start_server mine server
    pid=$(<"$BATS_TEST_TMPDIR/mine/xrootd.pid")
    stop_servers
    run ! is_running "$pid"
}

@test "stop_servers does not stop the servers of the file" {
    start_server mine server
    stop_servers
    run -0 server_is_up "$OTHER_URL"
}

@test "stop_server stops only the given server" {
    start_server mine server
    start_server yours server
    stop_server mine
    run -0 server_is_up "$YOURS_URL"
}

@test "stop_server stops the process of the server" {
    local pid
    start_server mine server
    pid=$(<"$BATS_TEST_TMPDIR/mine/xrootd.pid")
    stop_server mine
    run ! is_running "$pid"
}

@test "no server answers on NO_SERVER_PORT" {
    run ! server_is_up "root://localhost:$NO_SERVER_PORT"
}

@test "the tpc snippet runs the xrdcp of the build" {
    start_server mine server "$SETUP_DIR/tpc.cfg"
    run -0 grep -F "ofs.tpc pgm $(dirname "$(command -v xrootd)")/xrdcp --server" "$MINE_LOG"
}

@test "restart_server keeps the port" {
    start_server mine server
    restart_server mine
    run -0 server_is_up "$MINE_URL"
}

@test "restart_server keeps the data" {
    start_server mine server
    echo 'seed' | xrdcp - "$MINE_URL//kept"
    restart_server mine
    run -0 xrdcp "$MINE_URL//kept" -
    assert_output 'seed'
}

@test "restart_server fails when the server cannot start again" {
    start_server mine server
    cat "$BATS_TEST_DIRNAME/broken.cfg" >> "$BATS_TEST_TMPDIR/mine/mine.cfg"
    run ! restart_server mine
}

@test "start_server fails when the configuration is not valid" {
    run ! start_server broken server broken.cfg
    assert_output --partial 'initialization failed'
}

@test "start_server leaves no process when the server fails to start" {
    run ! start_server broken server broken.cfg
    run ! pgrep -f -- "$BATS_TEST_TMPDIR/broken/"
}

@test "start_server refuses a second server with the same name" {
    start_server mine server
    run ! start_server mine server
}

@test "a long run directory moves the admin path out of it" {
    start_server "long-$(printf 'x%.0s' {1..64})" server
    run -0 test -L "$BATS_TEST_TMPDIR/long-$(printf 'x%.0s' {1..64})/admin"
}

@test "stop_servers removes the admin path outside of the run directory" {
    local name admin
    name="long-$(printf 'x%.0s' {1..64})"
    start_server "$name" server
    admin=$(readlink "$BATS_TEST_TMPDIR/$name/admin")
    stop_servers
    run ! test -e "$admin"
}

@test "each test has its own client log" {
    xrdfs "$SHARED_URL" query config version
    run -0 test -s "$BATS_TEST_TMPDIR/client.log"
}

@test "the client authenticates with unix" {
    xrdfs "$SHARED_URL" query config version
    run -0 grep -F 'Authenticated with unix' "$XRD_LOGFILE"
}

@test "the shared credentials are read-only" {
    # shellcheck disable=SC2016 # the inner bash expands the variable
    run ! bash -c 'echo >> "$CREDENTIALS/ca.pem"'
}

@test "no test can add a file to the shared credentials" {
    run ! touch "$CREDENTIALS/new"
}

@test "the shared host certificate names localhost and both loopback addresses" {
    run -0 openssl x509 -in "$CREDENTIALS/host.pem" -noout -ext subjectAltName
    assert_output --regexp 'DNS:localhost.*IP Address:127\.0\.0\.1.*IP Address:0:0:0:0:0:0:0:1'
}

@test "the client logs at the Debug level" {
    assert_equal "$XRD_LOGLEVEL" Debug
}
