#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

setup() {
    export TEST_BIN="$BATS_TEST_TMPDIR/bin"
    export TRACE_FILE="$BATS_TEST_TMPDIR/arguments"
    export ENV_FILE="$BATS_TEST_TMPDIR/environment"
    mkdir -p "$TEST_BIN"

    for executable in xrdfs xrdcp; do
        cp "$BATS_TEST_DIRNAME/fake-native-command.bash" "$TEST_BIN/$executable"
        chmod +x "$TEST_BIN/$executable"
    done
    export PATH="$TEST_BIN:$PATH"
}

@test "stat delegates to xrdfs and preserves its exit status" {
    export FAKE_STATUS=37
    run "$XRD" stat root://example.test//file

    assert_failure 37
    assert_output "delegated output"
    assert_equal "$(sed -n '1p' "$TRACE_FILE")" "stat"
    assert_equal "$(sed -n '2p' "$TRACE_FILE")" "root://example.test//file"
}

@test "cat drops the bytes flag and keeps every URL" {
    run "$XRD" cat -b root://example.test//one root://example.test//two

    assert_success
    assert_equal "$(sed -n '1p' "$TRACE_FILE")" "cat"
    assert_equal "$(sed -n '2p' "$TRACE_FILE")" "root://example.test//one"
    assert_equal "$(sed -n '3p' "$TRACE_FILE")" "root://example.test//two"
}

@test "ls translates compatible gfal options" {
    run "$XRD" ls -alH root://example.test//directory

    assert_success
    assert_equal "$(sed -n '1p' "$TRACE_FILE")" "ls"
    assert_equal "$(sed -n '2p' "$TRACE_FILE")" "-l"
    assert_equal "$(sed -n '3p' "$TRACE_FILE")" "-h"
    assert_equal "$(sed -n '4p' "$TRACE_FILE")" \
        "root://example.test//directory"
}

@test "copy delegates remote to local with translated options" {
    run "$XRD" copy -fp -n 04 -T 30 -K ADLER32 \
        root://example.test//file "$BATS_TEST_TMPDIR/file"

    assert_success
    assert_equal "$(sed -n '1p' "$TRACE_FILE")" "--force"
    assert_equal "$(sed -n '2p' "$TRACE_FILE")" "--path"
    assert_equal "$(sed -n '3p' "$TRACE_FILE")" "--streams"
    assert_equal "$(sed -n '4p' "$TRACE_FILE")" "4"
    assert_equal "$(sed -n '5p' "$TRACE_FILE")" "--cksum"
    assert_equal "$(sed -n '6p' "$TRACE_FILE")" "adler32"
    assert_equal "$(sed -n '7p' "$TRACE_FILE")" "root://example.test//file"
    assert_equal "$(sed -n '8p' "$TRACE_FILE")" "$BATS_TEST_TMPDIR/file"
    assert grep -q '^XRD_CPTIMEOUT=30$' "$ENV_FILE"
}

@test "xattr delegates only read-only get operations" {
    run "$XRD" xattr root://example.test//file user.name

    assert_success
    assert_equal "$(sed -n '1p' "$TRACE_FILE")" "xattr"
    assert_equal "$(sed -n '2p' "$TRACE_FILE")" "root://example.test//file"
    assert_equal "$(sed -n '3p' "$TRACE_FILE")" "get"
    assert_equal "$(sed -n '4p' "$TRACE_FILE")" "user.name"

    rm -f "$TRACE_FILE"
    run "$XRD" xattr root://example.test//file user.name=value
    assert_failure 2
    assert_output --partial "read-only"
    refute [ -e "$TRACE_FILE" ]
}

@test "common options become native XRootD environment variables" {
    export X509_USER_PROXY=/tmp/inherited-proxy
    export XrdSecCREDS=inherited-serialized-credential
    run "$XRD" stat -vv -t 45 -4 --cert /tmp/cert --key /tmp/key \
        --log-file /tmp/client.log root://example.test//file

    assert_success
    assert grep -q '^XRD_LOGLEVEL=Info$' "$ENV_FILE"
    assert grep -q '^XRD_REQUESTTIMEOUT=45$' "$ENV_FILE"
    assert grep -q '^XRD_NETWORKSTACK=IPv4$' "$ENV_FILE"
    assert grep -q '^X509_USER_CERT=/tmp/cert$' "$ENV_FILE"
    assert grep -q '^X509_USER_KEY=/tmp/key$' "$ENV_FILE"
    assert grep -q '^X509_USER_PROXY=unset$' "$ENV_FILE"
    assert grep -q '^XrdSecGSIUSERCERT=/tmp/cert$' "$ENV_FILE"
    assert grep -q '^XrdSecGSIUSERKEY=/tmp/key$' "$ENV_FILE"
    assert grep -q '^XrdSecGSIUSERPROXY=/tmp/cert$' "$ENV_FILE"
    assert grep -q '^XrdSecCREDS=unset$' "$ENV_FILE"
    assert grep -q '^XRD_HTTPCLIENTCERTFILE=/tmp/cert$' "$ENV_FILE"
    assert grep -q '^XRD_HTTPCLIENTKEYFILE=/tmp/key$' "$ENV_FILE"
    assert grep -q '^XRD_LOGFILE=/tmp/client.log$' "$ENV_FILE"
}

@test "https URLs are passed unchanged to native commands" {
    run "$XRD" stat https://example.test/data/file

    assert_success
    assert_equal "$(sed -n '1p' "$TRACE_FILE")" "stat"
    assert_equal "$(sed -n '2p' "$TRACE_FILE")" \
        "https://example.test/data/file"
}

@test "unsupported compatibility options fail before delegation" {
    run "$XRD" ls --time-style full-iso root://example.test//directory

    assert_failure 2
    assert_output --partial "requires native xrdfs output support"
    refute [ -e "$TRACE_FILE" ]
}

@test "top-level help states that the command name is provisional" {
    run "$XRD" --help

    assert_success
    assert_output --partial "name 'xrd'"
    assert_output --partial "provisional"
}
