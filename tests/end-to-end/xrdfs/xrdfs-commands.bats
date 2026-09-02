#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

readonly endpoint=root://localhost:11966

setup() {
    launch_xrootd xrdfs-commands.cfg xrdfs-commands
    sleep 0.5

    run bats_pipe -0 printf '0123456789abcdef\n' \| xrdcp - \
        "$endpoint//file"
}

teardown() {
    kill_pid_files 2>/dev/null || true
}

bats::on_failure() {
    print_log_files
}

@test "global options validate their arguments" {
    run -0 xrdfs --help
    assert_output --partial 'Available commands:'

    run -1 xrdfs
    assert_output --partial 'Usage:'

    run -1 xrdfs --unknown-option
    assert_output --partial 'Usage:'

    run -1 xrdfs 'root://:11966' stat //file
    assert_output --partial 'Usage:'

    run -1 xrdfs -6 -4 stat "$endpoint//file"
    assert_output 'xrdfs: -4 and -6 are mutually exclusive'

    run -1 xrdfs --debug=-1 stat "$endpoint//file"
    assert_output "xrdfs: invalid debug level '-1' (expected 0-3)"

    run -0 xrdfs -d 1 stat "$endpoint//file"
    run -0 xrdfs -d 3 stat "$endpoint//file"

    run xrdfs query opaque value
    assert_failure

    run -1 xrdfs "$endpoint" stat file:///tmp/file
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "interactive mode parses quoted and relative paths" {
    run bats_pipe -0 printf \
        "\nmkdir //dir\ncd //dir\nmkdir child\nstat './child'\nstat \"../file\"\ncd //\nstat ../../escape\ncd ../../escape\nhelp\nexit\n" \
        \| xrdfs "$endpoint"
    assert_output --partial 'Path:   /dir/child'
    assert_output --partial 'Path:   /file'
    assert_output --partial "escapes above root"
    assert_output --partial 'Available commands:'
    assert_output --partial 'Goodbye.'

    run bats_pipe printf "stat relative\nstat ../escape\nexit\n" \
        \| xrdfs --no-cwd "$endpoint"
    assert_output --partial "relative path 'relative' is disallowed"
}

@test "directory creation and listings exercise display modes" {
    run -0 xrdfs "$endpoint" mkdir -p -mrwxr-x--- //dir/sub

    run -0 xrdfs "$endpoint" ls -l -u -D -C //dir
    assert_output --partial 'root://'
    assert_output --partial 'sub'

    run -0 xrdfs "$endpoint" ls -R -Z -h //dir
    assert_output --partial 'sub'

    run -0 xrdfs "$endpoint" ls -l //file
    assert_output --partial '//file'

    run -0 xrdfs "$endpoint" ls -l -h //file
    assert_output --partial '//file'

    run -0 xrdfs "$endpoint" ls -l -h //dir
    assert_output --partial 'sub'

    run -0 xrdfs "$endpoint" ls -u //file
    assert_output --partial 'root://'

    run -0 xrdfs "$endpoint" ls

    run -0 xrdfs "$endpoint" ls -l -u -R -D -Z -C //dir
    assert_output --partial 'root://'
    assert_output --partial 'sub'

    run xrdfs "$endpoint" ls relative
    assert_failure
    assert_output --partial "relative path 'relative' is disallowed"

    run xrdfs "$endpoint" ls //missing
    assert_failure

    run xrdfs "$endpoint" ls -Z //file
    assert_failure

    run xrdfs "$endpoint" mkdir
    assert_failure

    run xrdfs "$endpoint" mkdir -p
    assert_failure

    run xrdfs "$endpoint" mkdir -minvalid //invalid
    assert_failure

    run xrdfs "$endpoint" mkdir //file
    assert_failure
}

@test "mode conversion covers valid and invalid permission groups" {
    run -0 xrdfs "$endpoint" chmod //file rwxrwxrwx
    run -0 xrdfs "$endpoint" chmod //file ---------

    for mode in q-------- ---q----- ------q-- short; do
        run xrdfs "$endpoint" chmod //file "$mode"
        assert_failure
        assert_output --partial 'Invalid arguments'
    done

    run xrdfs "$endpoint" chmod //missing rwxr-x---
    assert_failure

    run xrdfs "$endpoint" chmod //file
    assert_failure

    run xrdfs "$endpoint" chmod relative rwxr-x---
    assert_failure
}

@test "rename remove and rmdir cover lifecycle failures and success" {
    run -0 xrdfs "$endpoint" mkdir -p //tree/child
    run bats_pipe -0 printf one \| xrdcp - "$endpoint//one"
    run bats_pipe -0 printf two \| xrdcp - "$endpoint//two"

    run -0 xrdfs "$endpoint" mv //one //renamed

    run xrdfs "$endpoint" mv //tree //tree/child/moved
    assert_failure
    assert_output --partial 'cannot move directory to a subdirectory of itself'

    run xrdfs "$endpoint" rmdir //tree
    assert_failure

    run -0 xrdfs "$endpoint" rm //renamed //two
    assert_output --partial 'rm //renamed'
    assert_output --partial 'rm //two'

    run -0 xrdfs "$endpoint" rmdir //tree/child
    run -0 xrdfs "$endpoint" rmdir //tree

    run xrdfs "$endpoint" rm
    assert_failure
    run xrdfs "$endpoint" rmdir
    assert_failure
    run xrdfs "$endpoint" mv //file
    assert_failure

    run xrdfs "$endpoint" mv //missing //destination
    assert_failure

    run xrdfs "$endpoint" mv relative //destination
    assert_failure

    run xrdfs "$endpoint" mv //file relative
    assert_failure

    run xrdfs "$endpoint" rmdir relative
    assert_failure

    run xrdfs "$endpoint" rm //missing
    assert_failure

    run xrdfs "$endpoint" rm relative
    assert_failure
}

@test "truncate validates sizes and reports remote failures" {
    run -0 xrdfs "$endpoint" truncate //file 5
    run -0 xrdfs "$endpoint" stat //file
    assert_output --partial 'Size:   5'

    run xrdfs "$endpoint" truncate //file invalid
    assert_failure
    assert_output --partial 'Invalid arguments'

    run xrdfs "$endpoint" truncate //missing 1
    assert_failure

    run xrdfs "$endpoint" truncate //file
    assert_failure

    run xrdfs "$endpoint" truncate relative 1
    assert_failure
}

@test "locate handles shallow deep and invalid requests" {
    run -0 xrdfs "$endpoint" locate -n -r //file
    assert_output --partial 'Server'

    run -0 xrdfs "$endpoint" locate -m -i //file
    assert_output --partial 'Server'

    run -0 xrdfs "$endpoint" locate -d -p //file
    assert_output --partial 'Server'

    run xrdfs "$endpoint" locate '*'
    refute_output ''

    run xrdfs "$endpoint" locate //file //other
    assert_failure
    assert_output --partial 'Invalid arguments'

    run xrdfs "$endpoint" locate //missing
    assert_failure

    run xrdfs "$endpoint" locate relative
    assert_failure

    run xrdfs "$endpoint" locate -n -r -d -m //file
    assert_failure
}

@test "stat evaluates flag queries and multiple paths" {
    run -0 xrdfs "$endpoint" mkdir //dir

    run -0 xrdfs "$endpoint" stat //file //dir
    assert_output --partial 'Path:   //file'
    assert_output --partial 'Path:   //dir'

    run -0 xrdfs "$endpoint" stat -q 'IsReadable&IsWritable' //file
    run -0 xrdfs "$endpoint" stat -q 'IsDir|Offline' //dir

    run xrdfs "$endpoint" stat -q 'IsDir&IsReadable' //file
    assert_failure

    run xrdfs "$endpoint" stat -q 'Offline|POSCPending' //file
    assert_failure

    run xrdfs "$endpoint" stat -q Unknown //file
    assert_failure

    run xrdfs "$endpoint" stat -q
    assert_failure

    run xrdfs "$endpoint" stat
    assert_failure

    run xrdfs "$endpoint" stat //missing
    assert_failure
}

@test "filesystem and server queries cover supported and rejected codes" {
    run -0 xrdfs "$endpoint" statvfs //
    assert_output --partial 'Nodes with RW space:'

    run xrdfs "$endpoint" statvfs relative
    assert_failure

    run xrdfs "$endpoint" statvfs
    assert_failure

    run -0 xrdfs "$endpoint" query config version
    run -0 xrdfs "$endpoint" query stats a

    for code in checksum checksumcancel opaque opaquefile space xattr prepare; do
        run xrdfs "$endpoint" query "$code" //file
        # The local server need not implement every query. Reaching it exercises
        # both successful responses and the xrdfs remote-error path.
        refute_output ''
    done

    run xrdfs "$endpoint" query invalid value
    assert_failure

    run xrdfs "$endpoint" query config
    assert_failure

    run xrdfs "$endpoint" query config version extra
    assert_failure

    run xrdfs "$endpoint" query prepare request-id //file relative
    assert_failure

    run xrdfs "$endpoint" query checksum relative
    assert_failure
}

@test "prepare covers flags responses and argument validation" {
    run -0 xrdfs "$endpoint" prepare -c -f -s -w -p 3 //file
    refute_output ''

    run -0 xrdfs "$endpoint" prepare -e //file

    run xrdfs "$endpoint" prepare -p 4 //file
    assert_failure

    run xrdfs "$endpoint" prepare -p invalid //file
    assert_failure

    run xrdfs "$endpoint" prepare -p
    assert_failure

    run xrdfs "$endpoint" prepare -a
    assert_failure

    run xrdfs "$endpoint" prepare -c
    assert_failure

    run xrdfs "$endpoint" prepare
    assert_failure

    run xrdfs "$endpoint" prepare -a request-id
    assert_failure
}

@test "cat copies to stdout and local files and validates output use" {
    run bats_pipe -0 printf second \| xrdcp - "$endpoint//second"

    run -0 xrdfs "$endpoint" cat //file //second
    assert_output --partial '0123456789abcdef'
    assert_output --partial 'second'

    run -0 xrdfs "$endpoint" cat -o "$BATS_TEST_TMPDIR/output" //file
    run -0 cmp "$BATS_TEST_TMPDIR/output" \
        "$BATS_TEST_TMPDIR/xrdfs-commands/file"

    run xrdfs "$endpoint" cat -o "$BATS_TEST_TMPDIR/output" //file //second
    assert_failure

    run xrdfs "$endpoint" cat -o
    assert_failure

    run xrdfs "$endpoint" cat //missing
    assert_failure

    run xrdfs "$endpoint" cat relative
    assert_failure

    run xrdfs "$endpoint" cat
    assert_failure
}

@test "tail handles offsets and malformed requests" {
    run -0 xrdfs "$endpoint" tail -c 5 //file
    assert_output 'cdef'

    run -0 xrdfs "$endpoint" tail -c 100 //file
    assert_output '0123456789abcdef'

    run xrdfs "$endpoint" tail -c invalid //file
    assert_failure

    run xrdfs "$endpoint" tail -c
    assert_failure

    run xrdfs "$endpoint" tail //missing
    assert_failure

    run xrdfs "$endpoint" tail -f //missing
    assert_failure

    run xrdfs "$endpoint" tail relative
    assert_failure

    run xrdfs "$endpoint" tail
    assert_failure

    run xrdfs "$endpoint" tail -c 1 -f //file extra
    assert_failure
}

@test "space and cache commands report success and server errors" {
    run -0 xrdfs "$endpoint" spaceinfo //
    assert_output --partial 'Largest free chunk:'

    run xrdfs "$endpoint" spaceinfo
    assert_failure

    run xrdfs "$endpoint" spaceinfo invalid-space
    refute_output ''

    run xrdfs "$endpoint" cache invalid //file
    assert_failure
    assert_output --partial 'Invalid arguments'

    run xrdfs "$endpoint" cache evict //file
    assert_failure

    run xrdfs "$endpoint" cache evict
    assert_failure

    run xrdfs "$endpoint" cache fevict relative
    assert_failure
}

@test "xattr supports its complete lifecycle and validates operands" {
    run -0 xrdfs "$endpoint" xattr //file set user.key=value

    run -0 xrdfs "$endpoint" xattr //file get user.key
    assert_output --partial 'user.key="value"'

    run -0 xrdfs "$endpoint" xattr //file list
    assert_output --partial 'user.key="value"'

    run -0 xrdfs "$endpoint" xattr //file del user.key

    run xrdfs "$endpoint" xattr //file get user.key
    assert_failure

    run xrdfs "$endpoint" xattr //file invalid
    assert_failure

    run xrdfs "$endpoint" xattr //file set
    assert_failure

    run xrdfs "$endpoint" xattr //file list extra
    assert_failure

    run xrdfs "$endpoint" xattr //file
    assert_failure

    run xrdfs "$endpoint" xattr //file get
    assert_failure

    run xrdfs "$endpoint" xattr //file del
    assert_failure

    run xrdfs "$endpoint" xattr //missing set user.key=value
    assert_failure

    run xrdfs "$endpoint" xattr //missing del user.key
    assert_failure

    run xrdfs "$endpoint" xattr //missing list
    assert_failure

    run xrdfs "$endpoint" xattr relative list
    assert_failure
}

@test "cd accepts directories and rejects files and missing paths" {
    run -0 xrdfs "$endpoint" mkdir //dir
    run -0 xrdfs "$endpoint" cd //dir

    run xrdfs "$endpoint" cd //file
    assert_failure

    run xrdfs "$endpoint" cd //missing
    assert_failure

    run xrdfs "$endpoint" cd
    assert_failure
}
