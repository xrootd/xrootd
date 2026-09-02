#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

setup() {
    export XRDFS=${XRDFS:-xrdfs}

    mkdir -p "$BATS_TEST_TMPDIR/xrdfs-full-url/data/subdir"
    printf 'first' > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/first.txt"
    printf 'second' > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/second.txt"
    : > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/empty.txt"
    printf 'hidden' > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/.hidden.txt"
    printf 'nested' \
        > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/subdir/nested.txt"
    dd if=/dev/zero \
        of="$BATS_TEST_TMPDIR/xrdfs-full-url/data/1025-bytes.dat" \
        bs=1025 count=1 2>/dev/null
    printf '\000\001\177\200\377' \
        > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/binary.dat"
    printf 'dash' > "$BATS_TEST_TMPDIR/xrdfs-full-url/-dash.txt"

    launch_xrootd xrdfs-full-url.cfg xrdfs-full-url

    export TEST_ENDPOINT=root://localhost:11965
    export TEST_DIRECTORY=$TEST_ENDPOINT//data/
    export TEST_SUBDIRECTORY=$TEST_ENDPOINT//data/subdir/
    export TEST_FILE=$TEST_ENDPOINT//data/first.txt
    export TEST_SECOND_FILE=$TEST_ENDPOINT//data/second.txt
    export TEST_EMPTY_FILE=$TEST_ENDPOINT//data/empty.txt
    export TEST_BINARY_FILE=$TEST_ENDPOINT//data/binary.dat

    local ready=false
    for _ in {1..50}; do
        if "$XRDFS" "$TEST_ENDPOINT" stat /data/first.txt \
            >/dev/null 2>&1; then
            ready=true
            break
        fi
        sleep 0.1
    done
    "$ready"

    # Seed one attribute on the ephemeral local fixture. The commands under
    # test only use the read-only list/get forms.
    "$XRDFS" "$TEST_ENDPOINT" xattr /data/first.txt set \
        user.test=fixture >/dev/null
}

teardown() {
    # The server can exit between the last command and teardown. Cleanup must
    # remain idempotent when a recorded PID has already disappeared.
    kill_pid_files 2>/dev/null || true
}

bats::on_failure() {
    print_log_files
}

@test "legacy and complete-URL stat forms have identical output" {
    run "$XRDFS" "$TEST_ENDPOINT" stat /data/first.txt
    assert_success
    local legacy_output=$output

    run "$XRDFS" stat "$TEST_FILE"
    assert_success
    assert_output "$legacy_output"
    assert_output --partial 'Size:   5'
}

@test "legacy and complete-URL ls forms have identical output" {
    run "$XRDFS" "$TEST_ENDPOINT" ls -l /data/
    assert_success
    local legacy_output=$output

    run "$XRDFS" ls -l "$TEST_DIRECTORY"
    assert_success
    assert_output "$legacy_output"
    assert_output --partial first.txt
}

@test "ls accepts gfal human-readable and directory options" {
    run "$XRDFS" ls -lH "$TEST_DIRECTORY"
    assert_success
    assert_output --partial first.txt
    assert_output --partial '1.1K'

    run "$XRDFS" ls --long --human-readable --directory "$TEST_DIRECTORY"
    assert_success
    assert_output --partial /data/
    refute_output --partial first.txt
}

@test "ls accepts gfal all and uncolored options as no-ops" {
    run "$XRDFS" ls "$TEST_DIRECTORY"
    assert_success
    local native_output=$output
    assert_output --partial .hidden.txt

    run "$XRDFS" ls -a "$TEST_DIRECTORY"
    assert_success
    assert_output "$native_output"

    run "$XRDFS" ls --all --color=never "$TEST_DIRECTORY"
    assert_success
    assert_output "$native_output"

    run "$XRDFS" ls --color never "$TEST_DIRECTORY"
    assert_success
    assert_output "$native_output"

    run "$XRDFS" ls -la "$TEST_DIRECTORY"
    assert_success
    assert_output --partial .hidden.txt
}

@test "ls rejects unsupported gfal presentation options" {
    for option in --time-style=full-iso --full-time --color=auto \
        --unknown-option; do
        run "$XRDFS" ls "$option" "$TEST_DIRECTORY"
        assert_failure
        assert_output --partial 'Invalid arguments'
    done

    run "$XRDFS" ls --color always "$TEST_DIRECTORY"
    assert_failure
    assert_output --partial 'Invalid arguments'
}

@test "ls appends repeatable gfal xattrs only to long output" {
    run "$XRDFS" ls --xattr missing.attribute "$TEST_FILE"
    assert_success
    assert_output /data/first.txt

    run "$XRDFS" ls -l --xattr user.status \
        --xattr=user.checksum.adler32 "$TEST_FILE"
    assert_success
    assert_output --regexp $'\tONLINE\t[[:xdigit:]]{8}$'

    run "$XRDFS" ls -l --xattr missing.attribute "$TEST_FILE"
    assert_failure

    local listing="$BATS_TEST_TMPDIR/ls-xattrs.out"
    run bash -c '"$1" ls -l --xattr user.status \
        --xattr user.checksum.adler32 "$2" >"$3"' \
        _ "$XRDFS" "$TEST_SUBDIRECTORY" "$listing"
    assert_success
    run awk -F '\t' '
        NF != 3 || $2 != "ONLINE" || $3 !~ /^[[:xdigit:]]{8}$/ { exit 1 }
        END { if (NR == 0) exit 1 }
    ' "$listing"
    assert_success
}

@test "ls option delimiter preserves dash-prefixed paths" {
    run bash -c \
        'printf "cd /\nls -- -dash.txt\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    assert_output --partial /-dash.txt
}

@test "legacy and complete-URL cat forms have identical output" {
    run "$XRDFS" "$TEST_ENDPOINT" cat /data/first.txt
    assert_success
    local legacy_output=$output

    run "$XRDFS" cat "$TEST_FILE"
    assert_success
    assert_output "$legacy_output"
    assert_output first
}

@test "cat accepts gfal bytes options and multiple same-endpoint URLs" {
    run "$XRDFS" cat -b "$TEST_FILE" "$TEST_SECOND_FILE"
    assert_success
    assert_output firstsecond

    run "$XRDFS" cat --bytes "$TEST_EMPTY_FILE"
    assert_success
    assert_output ''

    local destination="$BATS_TEST_TMPDIR/binary-download.dat"
    run bash -c '"$1" cat -b "$2" > "$3"' \
        _ "$XRDFS" "$TEST_BINARY_FILE" "$destination"
    assert_success
    run cmp "$BATS_TEST_TMPDIR/xrdfs-full-url/data/binary.dat" "$destination"
    assert_success
}

@test "cat bytes compatibility option still requires a file" {
    run "$XRDFS" "$TEST_ENDPOINT" cat -b
    assert_failure
    assert_output --partial 'Invalid arguments'
}

@test "cat option delimiter preserves dash-prefixed paths" {
    run bash -c \
        'printf "cd /\ncat -- -dash.txt\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    assert_output --partial dash
}

@test "xattr preserves explicit native list and get forms" {
    run "$XRDFS" "$TEST_ENDPOINT" xattr /data/first.txt list
    assert_success
    local list_output=$output

    run "$XRDFS" xattr "$TEST_FILE" list
    assert_success
    assert_output "$list_output"
    assert_output --partial 'user.test="fixture"'

    run "$XRDFS" "$TEST_ENDPOINT" xattr /data/first.txt get user.test
    assert_success
    local get_output=$output

    run "$XRDFS" xattr "$TEST_FILE" get user.test
    assert_success
    assert_output "$get_output"
}

@test "xattr preserves native set and delete forms" {
    run "$XRDFS" xattr "$TEST_FILE" set user.roundtrip=value=with=equals
    assert_success

    run "$XRDFS" xattr "$TEST_FILE" get user.roundtrip
    assert_success
    assert_output --partial 'user.roundtrip="value=with=equals"'

    run "$XRDFS" xattr "$TEST_FILE" del user.roundtrip
    assert_success

    run "$XRDFS" xattr "$TEST_FILE" get user.roundtrip
    assert_failure
}

@test "xattr shorthand maps gfal virtual attributes to native queries" {
    run "$XRDFS" "$TEST_ENDPOINT" query checksum /data/first.txt
    assert_success
    local checksum=$output

    run "$XRDFS" xattr "$TEST_FILE" xroot.cksum
    assert_success
    assert_output "$checksum"

    run "$XRDFS" xattr "$TEST_FILE" user.checksum.adler32
    assert_success
    assert_output "${checksum#* }"

    run "$XRDFS" xattr "$TEST_FILE" user.status
    assert_success
    assert_output ONLINE

    run "$XRDFS" xattr "$TEST_FILE"
    assert_success
    assert_output --partial "xroot.cksum = $checksum"
    assert_output --partial 'xroot.space = '
    assert_output --partial 'xroot.xattr '
    assert_output --partial 'spacetoken = { "totalsize": '
    assert_output --partial '"unusedsize": '
    assert_output --partial '"usedsize": '
    assert_output --partial '"guaranteedsize": '
}

@test "xattr shorthand falls back to native attributes and handles reserved names" {
    run "$XRDFS" xattr "$TEST_FILE" user.test
    assert_success
    assert_output fixture

    for attribute in list get set del; do
        run "$XRDFS" xattr "$TEST_FILE" set "$attribute=reserved-$attribute"
        assert_success

        run "$XRDFS" xattr "$TEST_FILE" -- "$attribute"
        assert_success
        assert_output "reserved-$attribute"
    done
}

@test "xattr rejects a checksum attribute without an algorithm" {
    run "$XRDFS" xattr "$TEST_FILE" user.checksum.
    assert_failure
    assert_output --partial 'Checksum type cannot be empty'
}

@test "xattr rejects an option delimiter without an attribute" {
    run "$XRDFS" xattr "$TEST_FILE" --
    assert_failure
    assert_output --partial 'Invalid arguments'
}

@test "sum maps gfal positional arguments to the native checksum query" {
    run "$XRDFS" "$TEST_ENDPOINT" query checksum \
        '/data/first.txt?cks.type=adler32'
    assert_success
    local query_output=$output

    run "$XRDFS" sum "$TEST_FILE" ADLER32
    assert_success
    assert_output "$query_output"
    assert_output --regexp '^adler32 [[:xdigit:]]{8}$'

    run "$XRDFS" "$TEST_ENDPOINT" sum /data/first.txt adler32
    assert_success
    assert_output "$query_output"
}

@test "sum preserves existing URL parameters when selecting an algorithm" {
    run "$XRDFS" sum "$TEST_FILE?xrdcl.test=1" ADLER32
    assert_success
    assert_output --regexp '^adler32 [[:xdigit:]]{8}$'
}

@test "sum rejects invalid and unsupported checksum types" {
    run "$XRDFS" sum "$TEST_FILE" 'md5&injected=true'
    assert_failure

    run "$XRDFS" sum "$TEST_FILE" sha256
    assert_failure
}

@test "URL parameters are preserved in the operand path" {
    run "$XRDFS" stat "$TEST_FILE?xrdcl.test=1"
    assert_success
    assert_output --partial 'Size:   5'
}

@test "mixed URL endpoints are rejected before execution" {
    run "$XRDFS" cat "$TEST_FILE" \
        root://127.0.0.1:11965//data/second.txt
    assert_failure 1
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "local and malformed URL operands are rejected" {
    for url in file:///tmp/examplefile FILE://localhost/tmp/examplefile \
        STDIO://-/examplefile '1root://localhost:11965//data/first.txt'; do
        run "$XRDFS" stat "$url"
        assert_failure 1
        assert_output 'xrdfs: invalid remote URL operand'
    done
}

@test "command-first raw queries remain unsupported" {
    run "$XRDFS" query "$TEST_FILE"
    assert_failure 1
    assert_output "xrdfs: command-first full URLs are not supported for 'query'"
}

@test "missing paths fail through the existing command handler" {
    run "$XRDFS" stat "$TEST_ENDPOINT//data/missing.txt"
    refute_output --partial 'invalid remote URL operand'
    assert_failure
}

@test "server-first and command-first syntax cannot be mixed" {
    run -1 "$XRDFS" "$TEST_ENDPOINT" stat "$TEST_FILE"
    assert_output --partial \
        'cannot mix a leading endpoint with full URL operands'
}

@test "URL-like xattr values remain command operands" {
    run -0 "$XRDFS" xattr "$TEST_FILE" set \
        link=https://example.org/resource

    run -0 "$XRDFS" xattr "$TEST_FILE" get link
    assert_output --partial 'link="https://example.org/resource"'
}

@test "multiple URLs on the same endpoint are normalized" {
    local renamed="$TEST_ENDPOINT//data/renamed"
    run -0 "$XRDFS" mv "$TEST_FILE" "$renamed"
    run -0 "$XRDFS" stat "$renamed"
}

@test "mixed URL credentials are rejected before execution" {
    run -1 "$XRDFS" mv \
        root://alice@localhost:11965//data/first.txt \
        root://bob@localhost:11965//data/renamed
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "invalid remote URLs are rejected" {
    run -1 "$XRDFS" stat root://:11965//data/first.txt
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "server-first query parameters may contain URL-like strings" {
    run "$XRDFS" "$TEST_ENDPOINT" query opaque "$TEST_FILE"
    refute_output --partial 'cannot mix a leading endpoint'
}

@test "debug levels follow xrdcp conventions" {
    run -0 "$XRDFS" -d 2 stat "$TEST_FILE"
    assert_output --partial 'Network Stack:'

    run -1 "$XRDFS" --debug 4 stat "$TEST_FILE"
    assert_output "xrdfs: invalid debug level '4' (expected 0-3)"
}

@test "IPv4 network stack can be selected" {
    run -0 "$XRDFS" -d 2 -4 stat "$TEST_FILE"
    assert_output --partial 'Network Stack: IPv4'
}

@test "IPv6 network stack can be selected" {
    run -0 "$XRDFS" -d 2 -6 stat "$TEST_FILE"
    assert_output --partial 'Network Stack: IPv6'
}

@test "network stack selections are mutually exclusive" {
    run -1 "$XRDFS" --ipv4 --ipv6 stat "$TEST_FILE"
    assert_output 'xrdfs: -4 and -6 are mutually exclusive'
}
