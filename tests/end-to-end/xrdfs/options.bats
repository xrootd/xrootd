#!/usr/bin/env bash

load ../setup/server.bash

setup() {
    start_server checksum.cfg
    echo 'full URL test' > "$SERVER_DATA/examplefile"
    echo 'dash-prefixed file' > "$SERVER_DATA/-b"
    mkdir "$SERVER_DATA/data"
    dd if=/dev/zero of="$SERVER_DATA/data/largefile" bs=1025 count=1 2>/dev/null
    echo 'hidden file' > "$SERVER_DATA/.hidden"
    echo 'dash-prefixed file' > "$SERVER_DATA/-dash"
}

teardown() {
    if [[ -d "$SERVER_DATA/0755" ]]; then
        chmod u+rwx "$SERVER_DATA/0755"
    fi
    stop_servers
}

local_mode() {
    if stat -c '%a' "$1" >/dev/null 2>&1; then
        stat -c '%a' "$1"
    else
        stat -f '%Lp' "$1"
    fi
}

@test "cat accepts byte aliases" {
    run -0 xrdfs cat -b $SERVER_URL//examplefile
    assert_output 'full URL test'

    run -0 xrdfs cat --bytes $SERVER_URL//examplefile
    assert_output 'full URL test'
}


@test "cat option delimiter preserves dash-prefixed paths" {
    run -0 xrdfs cat -- $SERVER_URL//-b
    assert_output 'dash-prefixed file'
}


@test "cat parses interspersed and attached options" {
    local target=$BATS_TEST_TMPDIR/cat-output
    run -0 xrdfs cat $SERVER_URL//examplefile -bo"$target"
    run -0 cat "$target"
    assert_output 'full URL test'
}


@test "cat rejects unknown options and missing output arguments" {
    run xrdfs cat $SERVER_URL//examplefile --unknown
    assert_failure
    assert_output --partial 'Invalid arguments'

    run xrdfs cat $SERVER_URL//examplefile -o
    assert_failure
    assert_output --partial 'Invalid arguments'
}


@test "cat resets option parsing after an interactive error" {
    run bats_pipe -0 printf \
        'cd /\ncat -bx\ncat -- -b\ncat --bytes examplefile\nexit\n' \
        \| xrdfs $SERVER_URL
    assert_output --partial 'Invalid arguments'
    assert_output --partial 'dash-prefixed file'
    assert_output --partial 'full URL test'
}


@test "ls accepts grouped and long display options" {
    run -0 xrdfs ls -lH $SERVER_URL//data
    assert_output --partial largefile
    assert_output --partial '1.1K'

    run -0 xrdfs ls --long --human-readable --directory \
        $SERVER_URL//data
    assert_output --partial /data
    refute_output --partial largefile
}


@test "ls accepts visibility and color options" {
    run -0 xrdfs ls $SERVER_URL//
    local default_output=$output
    assert_output --partial .hidden

    run -0 xrdfs ls -a $SERVER_URL//
    assert_output "$default_output"

    run -0 xrdfs ls --all --color=never $SERVER_URL//
    assert_output "$default_output"

    run -0 xrdfs ls --color never $SERVER_URL//
    assert_output "$default_output"

    run -0 xrdfs ls -1a $SERVER_URL//
    assert_output "$default_output"
}


@test "ls rejects unsupported options" {
    run xrdfs ls --color=auto $SERVER_URL//
    assert_output --partial 'Invalid arguments'

    run xrdfs ls --unknown-option $SERVER_URL//
    assert_output --partial 'Invalid arguments'

    run xrdfs ls -x $SERVER_URL//
    assert_output --partial 'Invalid arguments'
}


@test "ls option delimiter preserves dash-prefixed paths" {
    run bash -c \
        'printf "cd /\nls -- -dash\nexit\n" | xrdfs "$1"' bash "$SERVER_URL"
    assert_success
    assert_output --partial /-dash
}


@test "ls accepts options after operands with POSIXLY_CORRECT" {
    run -0 env POSIXLY_CORRECT=1 xrdfs ls \
        $SERVER_URL//data -lH --color=never
    assert_output --partial '1.1K'
    assert_output --partial largefile
}


@test "ls resets grouped option parsing after an interactive error" {
    run bats_pipe -0 printf \
        'ls -lxh /data\nls /data -lH\nls --color\nls -- -dash\nexit\n' \
        \| xrdfs $SERVER_URL
    assert_output --partial 'Invalid arguments'
    assert_output --partial '1.1K'
    assert_output --partial /-dash
}


@test "cksum selects algorithms using short and long options" {
    local algorithm query_output
    for algorithm in adler32 crc32c md5; do
        run -0 xrdfs $SERVER_URL query checksum \
            "/examplefile?cks.type=$algorithm"
        query_output=$output

        run -0 xrdfs cksum -a "$algorithm" $SERVER_URL//examplefile
        assert_output "$query_output"

        run -0 xrdfs cksum --algorithm="$algorithm" \
            $SERVER_URL//examplefile
        assert_output "$query_output"

        run -0 xrdfs $SERVER_URL cksum \
            --algorithm "$algorithm" /examplefile
        assert_output "$query_output"
    done
}


@test "cksum defaults to the server algorithm" {
    run -0 xrdfs $SERVER_URL query checksum /examplefile
    local query_output=$output
    run -0 xrdfs cksum $SERVER_URL//examplefile
    assert_output "$query_output"
}


@test "cksum preserves URL parameters and accepts interspersed options" {
    run -0 env POSIXLY_CORRECT=1 xrdfs cksum \
        "$SERVER_URL//examplefile?xrdcl.test=1" -aADLER32
    assert_output --regexp '^adler32 [[:xdigit:]]{8}$'
}


@test "cksum preserves leading zeros in the server digest" {
    run bats_pipe -0 printf a \| xrdcp - $SERVER_URL//short
    run -0 xrdfs cksum -a adler32 $SERVER_URL//short
    assert_output 'adler32 00620062'
}


@test "cksum rejects invalid options operands and algorithms" {
    local option
    for option in --unknown -a --algorithm --algorithm= '--algorithm=bad&type'; do
        run xrdfs cksum $SERVER_URL//examplefile "$option"
        assert_failure
        assert_output --partial 'Invalid arguments'
    done
    run xrdfs $SERVER_URL cksum -a md5
    assert_failure
    run xrdfs cksum $SERVER_URL//examplefile md5
    assert_failure
    run xrdfs cksum -a not_a_checksum $SERVER_URL//examplefile
    assert_failure
    run xrdfs cksum $SERVER_URL//missing
    assert_failure
}


@test "cksum resets parsing between interactive commands and handles delimiters" {
    run -0 xrdfs $SERVER_URL query checksum '/-b?cks.type=md5'
    local checksum=$output
    run bats_pipe -0 printf \
        'cksum -ax -z\ncksum --algorithm=md5 -- -b\ncksum --algorithm\ncksum /examplefile -a adler32\nexit\n' \
        \| xrdfs $SERVER_URL
    assert_output --partial 'Invalid arguments'
    assert_output --partial "$checksum"
    assert_output --regexp 'adler32 [[:xdigit:]]{8}'
}


@test "xattr queries virtual attributes" {
    run -0 xrdfs $SERVER_URL query checksum /examplefile
    local checksum=$output

    run -0 xrdfs xattr $SERVER_URL//examplefile xroot.cksum
    assert_output "$checksum"

    run -0 xrdfs xattr $SERVER_URL//examplefile \
        user.checksum.adler32
    assert_output "${checksum#* }"

    run -0 xrdfs xattr $SERVER_URL//examplefile user.status
    assert_output ONLINE
}


@test "xattr lists the fixed virtual attributes" {
    run -0 xrdfs xattr $SERVER_URL//examplefile
    assert_output --partial 'xroot.cksum = adler32 '
    assert_output --partial 'xroot.space = '
    assert_output --partial 'xroot.xattr '
    assert_output --partial 'spacetoken = { "totalsize": '
}


@test "xattr succeeds when only some virtual attributes resolve" {
    run -0 xrdfs xattr $SERVER_URL//data
    assert_output --partial 'xroot.cksum FAILED:'
    assert_output --partial 'xroot.space = '
}


@test "xattr fails when all virtual attribute queries fail" {
    stop_server server
    run env XRD_CONNECTIONWINDOW=1 XRD_CONNECTIONRETRY=1 \
        XRD_REQUESTTIMEOUT=1 XRD_STREAMTIMEOUT=1 \
        xrdfs xattr $SERVER_URL//examplefile
    assert_failure
    local attribute
    for attribute in xroot.cksum xroot.space xroot.xattr spacetoken; do
        assert_output --partial "$attribute FAILED:"
        refute_output --partial "$attribute = "
    done
}


@test "xattr shorthand falls back to native attributes" {
    run -0 xrdfs xattr $SERVER_URL//examplefile set \
        user.short=value

    run -0 xrdfs xattr $SERVER_URL//examplefile user.short
    assert_output value

    run -0 xrdfs xattr $SERVER_URL//examplefile -- user.short
    assert_output value
}


@test "mkdir accepts separated and long mode options" {
    run -0 xrdfs mkdir -p -m 0755 \
        $SERVER_URL//mkdir/separated/a \
        $SERVER_URL//mkdir/separated/b
    run -0 xrdfs stat $SERVER_URL//mkdir/separated/a
    run -0 xrdfs stat $SERVER_URL//mkdir/separated/b

    run -0 xrdfs mkdir --parents --mode=0700 \
        $SERVER_URL//mkdir/long/child
    run -0 xrdfs stat $SERVER_URL//mkdir/long/child
}


@test "mkdir preserves attached and symbolic mode forms" {
    run -0 xrdfs mkdir -p -m0755 \
        $SERVER_URL//mkdir/attached/child
    run -0 xrdfs mkdir -p -mrwxr-x--- \
        $SERVER_URL//mkdir/symbolic/child
}


@test "mkdir accepts grouped and interspersed options" {
    run -0 env POSIXLY_CORRECT=1 xrdfs mkdir \
        $SERVER_URL//mkdir/grouped/first -pm0700 \
        $SERVER_URL//mkdir/grouped/second
    run -0 local_mode "$SERVER_DATA/mkdir/grouped/first"
    assert_output 700
    run -0 local_mode "$SERVER_DATA/mkdir/grouped/second"
    assert_output 700
}


@test "mkdir validates all options before creating paths" {
    local option
    for option in --unknown -m --mode --mode= --mode=0788 \
        --mode=1000 --mode=40000000000 --mode=777777777777777777777777; do
        run xrdfs mkdir $SERVER_URL//not-created "$option"
        assert_failure
        [[ ! -e "$SERVER_DATA/not-created" ]]
    done
    run xrdfs mkdir -m invalid -m 0700 $SERVER_URL//not-created
    assert_failure
    [[ ! -e "$SERVER_DATA/not-created" ]]
}


@test "mkdir resets parsing and accepts dash-prefixed modes and paths" {
    run bats_pipe -0 printf \
        'mkdir -pxm0700 invalid\nmkdir -- -directory\nmkdir --mode --------- zero-mode\nmkdir -pm0700 nested/child\nexit\n' \
        \| xrdfs $SERVER_URL
    assert_output --partial 'Invalid arguments'
    [[ -d "$SERVER_DATA/-directory" ]]
    [[ -d "$SERVER_DATA/nested/child" ]]
    run -0 local_mode "$SERVER_DATA/zero-mode"
    # The server always adds owner rwx when creating directories.
    assert_output 700
}


@test "chmod accepts mode-first and path-first forms" {
    local path=$SERVER_DATA/chmod-options
    local url=$SERVER_URL//chmod-options

    run -0 xrdfs mkdir "$url"

    run -0 xrdfs chmod 0715 "$url"
    run -0 local_mode "$path"
    assert_output 715

    run -0 xrdfs chmod "$url" rwxr-x---
    run -0 local_mode "$path"
    assert_output 750

    run -0 xrdfs $SERVER_URL chmod /chmod-options 0704
    run -0 local_mode "$path"
    assert_output 704
}


@test "chmod preserves dash-prefixed modes and mode-shaped path precedence" {
    run -0 xrdfs mkdir $SERVER_URL//0755
    run bats_pipe -0 printf \
        'chmod 0755 0700\nexit\n' \| xrdfs $SERVER_URL
    run -0 local_mode "$SERVER_DATA/0755"
    assert_output 700

    run -0 xrdfs $SERVER_URL chmod /0755 ---------
    run -0 local_mode "$SERVER_DATA/0755"
    assert_output 0
}
