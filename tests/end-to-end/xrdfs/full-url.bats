#!/usr/bin/env bash

load ../setup/server.bash

setup_file() {
    start_server server server
}

teardown_file() {
    stop_servers
}

# Each test has its own file, /t<number>
setup() {
    F="t${BATS_TEST_NUMBER}"
    echo 'full URL test' > "$SERVER_DATA/$F"
    HOST="localhost:$SERVER_PORT"
}

@test "legacy server-first syntax remains supported" {
    run -0 xrdfs root://$HOST stat //$F
}

@test "server-first and command-first syntax cannot be mixed" {
    run -1 xrdfs root://$HOST stat \
        root://$HOST//$F
    assert_output --partial 'cannot mix a leading endpoint with full URL operands'
}

@test "command-first syntax accepts a full URL" {
    run -0 xrdfs stat root://$HOST//$F
}

@test "subcommand options are not consumed as global options" {
    run -0 xrdfs ls -l root://$HOST//
    assert_output --partial "$F"
}

@test "URL parameters are preserved in the operand path" {
    run -0 xrdfs stat "root://$HOST//$F?xrdcl.test=1"
}

@test "URL-like xattr values remain command operands" {
    run -0 xrdfs xattr root://$HOST//$F set \
        link=https://example.org/resource

    run -0 xrdfs xattr root://$HOST//$F get link
    assert_output --partial 'link="https://example.org/resource"'
}

@test "multiple URLs on the same endpoint are normalized" {
    run -0 xrdfs mv root://$HOST//$F \
        root://$HOST//$F.renamed

    run -0 xrdfs stat root://$HOST//$F.renamed
}

@test "mixed URL endpoints are rejected before execution" {
    run -1 xrdfs mv root://$HOST//$F \
        root://127.0.0.1:$SERVER_PORT//$F.renamed
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "mixed URL credentials are rejected before execution" {
    run -1 xrdfs mv root://alice@$HOST//$F \
        root://bob@$HOST//$F.renamed
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "a file URL is rejected" {
    run -1 xrdfs stat file:///tmp/$F
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "a stdio URL is rejected" {
    run -1 xrdfs stat stdio:///$F
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "invalid remote URLs are rejected" {
    run -1 xrdfs stat root://:$SERVER_PORT//$F
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "server-first query parameters may contain URL-like strings" {
    run xrdfs root://$HOST query opaque \
        root://$HOST//$F
    refute_output --partial 'cannot mix a leading endpoint'
}

@test "command-first raw queries remain unsupported" {
    run -1 xrdfs query root://$HOST//$F
    assert_output "xrdfs: command-first full URLs are not supported for 'query'"
}

@test "debug level 2 reports the network stack" {
    run -0 env -u XRD_LOGFILE xrdfs -d 2 stat root://$HOST//$F
    assert_output --partial 'Network Stack:'
}

@test "debug level 4 is not valid, as in xrdcp" {
    run -1 xrdfs --debug 4 stat root://$HOST//$F
    assert_output "xrdfs: invalid debug level '4' (expected 0-3)"
}

@test "IPv4 network stack can be selected" {
    run -0 env -u XRD_LOGFILE xrdfs -d 2 -4 stat root://$HOST//$F
    assert_output --partial 'Network Stack: IPv4'
}

@test "IPv6 network stack can be selected" {
    run -0 env -u XRD_LOGFILE xrdfs -d 2 -6 stat root://$HOST//$F
    assert_output --partial 'Network Stack: IPv6'
}

@test "network stack selections are mutually exclusive" {
    run -1 xrdfs --ipv4 --ipv6 stat root://$HOST//$F
    assert_output 'xrdfs: -4 and -6 are mutually exclusive'
}
