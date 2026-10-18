#!/usr/bin/env bash

# Tests of the common server setup itself (see server.bash).

load server.bash

setup_file() {
    start_server shared
    start_server other
}

teardown_file() {
    stop_servers
}

# A test which fails before it stops its stand-in process must not leave it,
# else BATS waits for its output.
teardown() {
    [[ -z "${STANDIN:-}" ]] || kill "$STANDIN" 2>/dev/null || true
    stop_servers
}

@test "a server shared by the test file answers on its URL" {
    run -0 xrdfs "$SHARED_URL" query config version
}

@test "a server answers on its IPv4 URL" {
    run -0 xrdfs "$SHARED_URL4" query config version
}

@test "a server answers on its IPv6 URL" {
    require_ipv6
    run -0 xrdfs "$SHARED_URL6" query config version
}

@test "two servers of a test file use different ports" {
    assert_not_equal "$SHARED_PORT" "$OTHER_PORT"
}

@test "the data directory of a server is its local root" {
    echo 'seed' > "$SHARED_DATA/data-dir"
    run -0 xrdcp "$SHARED_URL//data-dir" -
    assert_output 'seed'
}

@test "the server log has the configuration of the role" {
    start_server mine
    run -0 grep -F "oss.localroot $BATS_TEST_TMPDIR/mine/data" "$MINE_LOG"
}

@test "each test starts with an empty log of a server of the test file" {
    run ! test -s "$SHARED_LOG"
}

@test "a server of a test runs below the test directory" {
    start_server mine
    assert_equal "$MINE_DATA" "$BATS_TEST_TMPDIR/mine/data"
}

@test "a server of a test can use the name of a server of the test file" {
    local file_url=$SHARED_URL
    start_server shared
    assert_not_equal "$SHARED_URL" "$file_url"
    echo 'of the test' > "$SHARED_DATA/marker"
    run -0 xrdfs "$SHARED_URL" stat /marker
}

@test "stop_servers stops the servers of the test" {
    local pid
    start_server mine
    pid=$(<"$BATS_TEST_TMPDIR/mine/xrootd.pid")
    stop_servers
    run ! is_running "$pid"
}

@test "stop_servers does not stop the servers of the test file" {
    start_server mine
    stop_servers
    run -0 server_is_up "$OTHER_URL"
}

@test "stop_server stops only the given server" {
    start_server mine
    start_server yours
    stop_server mine
    run -0 server_is_up "$YOURS_URL"
}

@test "stop_server stops the process of the server" {
    local pid
    start_server mine
    pid=$(<"$BATS_TEST_TMPDIR/mine/xrootd.pid")
    stop_server mine
    run ! is_running "$pid"
}

@test "no server answers on NO_SERVER_PORT" {
    run ! server_is_up "root://localhost:$NO_SERVER_PORT"
}

@test "the tpc snippet runs the xrdcp of the build" {
    start_server mine "$SETUP_DIR/tpc.cfg"
    run -0 grep -F "ofs.tpc pgm $(dirname "$(command -v xrootd)")/xrdcp --server" "$MINE_LOG"
}

@test "restart_server keeps the port" {
    start_server mine
    restart_server mine
    run -0 server_is_up "$MINE_URL"
}

@test "restart_server keeps the data" {
    start_server mine
    echo 'seed' | xrdcp - "$MINE_URL//kept"
    restart_server mine
    run -0 xrdcp "$MINE_URL//kept" -
    assert_output 'seed'
}

@test "restart_server fails when the server cannot start again" {
    start_server mine
    cat "$BATS_TEST_DIRNAME/broken.cfg" >> "$BATS_TEST_TMPDIR/mine/mine.cfg"
    run ! restart_server mine
}

@test "a restart on a port in use fails" {
    start_server mine
    echo "$SHARED_PORT" > "$BATS_TEST_TMPDIR/mine/port"
    run ! restart_server mine
}

@test "start_server fails when the configuration is not valid" {
    run ! start_server broken broken.cfg
    assert_output --partial 'initialization failed'
}

@test "start_server leaves no process when the server fails to start" {
    run ! start_server broken broken.cfg
    # status 1: no process matches; 2 and 3 are errors of pgrep
    run -1 pgrep -f -- "$BATS_TEST_TMPDIR/broken/"
}

@test "start_server refuses a second server with the same name" {
    start_server mine
    run ! start_server mine
}

@test "start_server without arguments starts the server named server" {
    start_server
    run -0 server_is_up "$SERVER_URL"
}

@test "start_server takes a snippet without the name and the role" {
    start_server tpc.cfg
    run -0 grep -F "ofs.tpc pgm" "$SERVER_LOG"
}

@test "start_server refuses a third word after the name and the role" {
    run ! start_server mine server-nosec other
    assert_output --partial "unexpected argument 'other'"
}

@test "a server runs with its name as instance name" {
    start_server named
    run -0 grep -F "xrootd named@" "$NAMED_LOG"
}

# With the same seed, start_server mine first tries the port of the server busy
@test "start_server takes another port when its port is in use" {
    RANDOM=42
    start_server busy
    RANDOM=42
    start_server mine
    run -0 grep -E 'Unable to (bind socket to port|listen on stream)' "$MINE_LOG"
    assert_not_equal "$MINE_PORT" "$BUSY_PORT"
}

# After a crash, the system can give the pid of the server to another process
@test "stop_servers spares a process which has the pid of a stopped server" {
    start_server mine
    stop_pid "$(<"$BATS_TEST_TMPDIR/mine/xrootd.pid")"
    sleep 30 3>&- 4>&- &
    STANDIN=$!
    echo "$STANDIN" > "$BATS_TEST_TMPDIR/mine/xrootd.pid"
    run -0 stop_servers
    run -0 is_running "$STANDIN"
}

@test "a long run directory moves the admin path out of it" {
    start_server "long-$(printf 'x%.0s' {1..30})"
    run -0 test -L "$BATS_TEST_TMPDIR/long-$(printf 'x%.0s' {1..30})/admin"
}

@test "stop_servers removes the admin path outside of the run directory" {
    local name admin
    name="long-$(printf 'x%.0s' {1..30})"
    start_server "$name"
    admin=$(readlink "$BATS_TEST_TMPDIR/$name/admin")
    stop_servers
    run ! test -e "$admin"
}

@test "each test has its own client log" {
    xrdfs "$SHARED_URL" query config version
    run -0 test -s "$BATS_TEST_TMPDIR/client.log"
}

@test "each test has its own home directory" {
    assert_equal "$HOME" "$BATS_TEST_TMPDIR"
}

@test "a test sees none of the credentials of the user" {
    assert_equal "$XDG_RUNTIME_DIR" /dev/null
    assert_equal "$KRB5CCNAME" FILE:/dev/null/krb5cc
    run -0 env
    refute_line --regexp '^(X509_USER_PROXY|X509_USER_CERT|X509_USER_KEY|BEARER_TOKEN|BEARER_TOKEN_FILE)='
}

@test "the client authenticates with unix" {
    xrdfs "$SHARED_URL" query config version
    run -0 grep -F 'Authenticated with unix' "$XRD_LOGFILE"
}

@test "the client logs at the Debug level or above" {
    assert_regex "$XRD_LOGLEVEL" '^(Debug|Dump)$'
}
