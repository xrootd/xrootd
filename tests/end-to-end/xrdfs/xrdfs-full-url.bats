#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

setup() {
    launch_xrootd xrdfs-full-url.cfg xrdfs-full-url
    sleep 0.5

    run bats_pipe -0 echo 'full URL test' \| xrdcp - \
        root://localhost:11965//examplefile
    run bats_pipe -0 echo 'dash-prefixed file' \| xrdcp - \
        root://localhost:11965//-b
    run -0 xrdfs root://localhost:11965 mkdir /data
    run bats_pipe -0 dd if=/dev/zero bs=1025 count=1 \| xrdcp - \
        root://localhost:11965//data/largefile
    run bats_pipe -0 echo 'hidden file' \| xrdcp - \
        root://localhost:11965//.hidden
    run bats_pipe -0 echo 'dash-prefixed file' \| xrdcp - \
        root://localhost:11965//-dash
}

teardown() {
    kill_pid_files 2>/dev/null || true
}

bats::on_failure() {
    print_log_files
}

local_mode() {
    if stat -c '%a' "$1" >/dev/null 2>&1; then
        stat -c '%a' "$1"
    else
        stat -f '%Lp' "$1"
    fi
}

@test "legacy server-first syntax remains supported" {
    run -0 xrdfs root://localhost:11965 stat //examplefile
}

@test "server-first and command-first syntax cannot be mixed" {
    run -1 xrdfs root://localhost:11965 stat \
        root://localhost:11965//examplefile
    assert_output --partial 'cannot mix a leading endpoint with full URL operands'
}

@test "command-first syntax accepts a full URL" {
    run -0 xrdfs stat root://localhost:11965//examplefile
}

@test "cat accepts byte aliases" {
    run -0 xrdfs cat -b root://localhost:11965//examplefile
    assert_output 'full URL test'

    run -0 xrdfs cat --bytes root://localhost:11965//examplefile
    assert_output 'full URL test'
}

@test "cat option delimiter preserves dash-prefixed paths" {
    run -0 xrdfs cat -- root://localhost:11965//-b
    assert_output 'dash-prefixed file'
}

@test "subcommand options are not consumed as global options" {
    run -0 xrdfs ls -l root://localhost:11965//
    assert_output --partial examplefile
}

@test "ls accepts grouped and long display options" {
    run -0 xrdfs ls -lH root://localhost:11965//data
    assert_output --partial largefile
    assert_output --partial '1.1K'

    run -0 xrdfs ls --long --human-readable --directory \
        root://localhost:11965//data
    assert_output --partial /data
    refute_output --partial largefile
}

@test "ls accepts visibility and color options" {
    run -0 xrdfs ls root://localhost:11965//
    local default_output=$output
    assert_output --partial .hidden

    run -0 xrdfs ls -a root://localhost:11965//
    assert_output "$default_output"

    run -0 xrdfs ls --all --color=never root://localhost:11965//
    assert_output "$default_output"

    run -0 xrdfs ls --color never root://localhost:11965//
    assert_output "$default_output"

    run -0 xrdfs ls -1a root://localhost:11965//
    assert_output "$default_output"
}

@test "ls rejects unsupported options" {
    run xrdfs ls --color=auto root://localhost:11965//
    assert_output --partial 'Invalid arguments'

    run xrdfs ls --unknown-option root://localhost:11965//
    assert_output --partial 'Invalid arguments'

    run xrdfs ls -x root://localhost:11965//
    assert_output --partial 'Invalid arguments'
}

@test "ls option delimiter preserves dash-prefixed paths" {
    run bash -c \
        'printf "cd /\nls -- -dash\nexit\n" | xrdfs root://localhost:11965'
    assert_success
    assert_output --partial /-dash
}

@test "sum selects the requested checksum algorithm" {
    run -0 xrdfs root://localhost:11965 query checksum \
        '/examplefile?cks.type=adler32'
    local query_output=$output

    run -0 xrdfs sum root://localhost:11965//examplefile ADLER32
    assert_output "$query_output"
    assert_output --regexp '^adler32 [[:xdigit:]]{8}$'

    run -0 xrdfs root://localhost:11965 sum /examplefile adler32
    assert_output "$query_output"
}

@test "sum preserves URL parameters" {
    run -0 xrdfs sum \
        'root://localhost:11965//examplefile?xrdcl.test=1' ADLER32
    assert_output --regexp '^adler32 [[:xdigit:]]{8}$'
}

@test "xattr queries virtual attributes" {
    run -0 xrdfs root://localhost:11965 query checksum /examplefile
    local checksum=$output

    run -0 xrdfs xattr root://localhost:11965//examplefile xroot.cksum
    assert_output "$checksum"

    run -0 xrdfs xattr root://localhost:11965//examplefile \
        user.checksum.adler32
    assert_output "${checksum#* }"

    run -0 xrdfs xattr root://localhost:11965//examplefile user.status
    assert_output ONLINE
}

@test "xattr lists the fixed virtual attributes" {
    run -0 xrdfs xattr root://localhost:11965//examplefile
    assert_output --partial 'xroot.cksum = adler32 '
    assert_output --partial 'xroot.space = '
    assert_output --partial 'xroot.xattr '
    assert_output --partial 'spacetoken = { "totalsize": '
}

@test "xattr shorthand falls back to native attributes" {
    run -0 xrdfs xattr root://localhost:11965//examplefile set \
        user.short=value

    run -0 xrdfs xattr root://localhost:11965//examplefile user.short
    assert_output value

    run -0 xrdfs xattr root://localhost:11965//examplefile -- user.short
    assert_output value
}

@test "mkdir accepts separated and long mode options" {
    run -0 xrdfs mkdir -p -m 0755 \
        root://localhost:11965//mkdir/separated/a \
        root://localhost:11965//mkdir/separated/b
    run -0 xrdfs stat root://localhost:11965//mkdir/separated/a
    run -0 xrdfs stat root://localhost:11965//mkdir/separated/b

    run -0 xrdfs mkdir --parents --mode=0700 \
        root://localhost:11965//mkdir/long/child
    run -0 xrdfs stat root://localhost:11965//mkdir/long/child
}

@test "mkdir preserves attached and symbolic mode forms" {
    run -0 xrdfs mkdir -p -m0755 \
        root://localhost:11965//mkdir/attached/child
    run -0 xrdfs mkdir -p -mrwxr-x--- \
        root://localhost:11965//mkdir/symbolic/child
}

@test "chmod accepts mode-first and path-first forms" {
    local path=$BATS_TEST_TMPDIR/xrdfs-full-url/chmod-options
    local url=root://localhost:11965//chmod-options

    run -0 xrdfs mkdir "$url"

    run -0 xrdfs chmod 0715 "$url"
    run -0 local_mode "$path"
    assert_output 715

    run -0 xrdfs chmod "$url" rwxr-x---
    run -0 local_mode "$path"
    assert_output 750

    run -0 xrdfs root://localhost:11965 chmod /chmod-options 0704
    run -0 local_mode "$path"
    assert_output 704
}

@test "URL parameters are preserved in the operand path" {
    run -0 xrdfs stat 'root://localhost:11965//examplefile?xrdcl.test=1'
}

@test "URL-like xattr values remain command operands" {
    run -0 xrdfs xattr root://localhost:11965//examplefile set \
        link=https://example.org/resource

    run -0 xrdfs xattr root://localhost:11965//examplefile get link
    assert_output --partial 'link="https://example.org/resource"'
}

@test "multiple URLs on the same endpoint are normalized" {
    run -0 xrdfs mv root://localhost:11965//examplefile \
        root://localhost:11965//renamed

    run -0 xrdfs stat root://localhost:11965//renamed
}

@test "mixed URL endpoints are rejected before execution" {
    run -1 xrdfs mv root://localhost:11965//examplefile \
        root://127.0.0.1:11965//renamed
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "mixed URL credentials are rejected before execution" {
    run -1 xrdfs mv root://alice@localhost:11965//examplefile \
        root://bob@localhost:11965//renamed
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "local URLs are rejected" {
    run -1 xrdfs stat file:///tmp/examplefile
    assert_output 'xrdfs: invalid remote URL operand'

    run -1 xrdfs stat stdio:///examplefile
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "invalid remote URLs are rejected" {
    run -1 xrdfs stat root://:11965//examplefile
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "server-first query parameters may contain URL-like strings" {
    run xrdfs root://localhost:11965 query opaque \
        root://localhost:11965//examplefile
    refute_output --partial 'cannot mix a leading endpoint'
}

@test "command-first raw queries remain unsupported" {
    run -1 xrdfs query root://localhost:11965//examplefile
    assert_output "xrdfs: command-first full URLs are not supported for 'query'"
}

@test "debug levels follow xrdcp conventions" {
    run -0 xrdfs -d 2 stat root://localhost:11965//examplefile
    assert_output --partial 'Network Stack:'

    run -1 xrdfs --debug 4 stat root://localhost:11965//examplefile
    assert_output "xrdfs: invalid debug level '4' (expected 0-3)"
}

@test "IPv4 network stack can be selected" {
    run -0 xrdfs -d 2 -4 stat root://localhost:11965//examplefile
    assert_output --partial 'Network Stack: IPv4'
}

@test "IPv6 network stack can be selected" {
    run -0 xrdfs -d 2 -6 stat root://localhost:11965//examplefile
    assert_output --partial 'Network Stack: IPv6'
}

@test "network stack selections are mutually exclusive" {
    run -1 xrdfs --ipv4 --ipv6 stat root://localhost:11965//examplefile
    assert_output 'xrdfs: -4 and -6 are mutually exclusive'
}
