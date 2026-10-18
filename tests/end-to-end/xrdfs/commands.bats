#!/usr/bin/env bash

# The commands of xrdfs: their results, and the checks of their arguments. The
# tests share one server, and each test has its own directory //t<number> with
# the file "file" in it.

load ../setup/server.bash

readonly CONTENT='0123456789abcdef'

setup_file() {
    start_server server server
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    URL=$SERVER_URL
    D="//t${BATS_TEST_NUMBER}"
    F="$D/file"
    mkdir "$SERVER_DATA/t${BATS_TEST_NUMBER}"
    echo "$CONTENT" > "$SERVER_DATA$F"
}

#
# global options
#

@test "--help lists the commands" {
    run -0 xrdfs --help
    assert_output --partial 'Available commands:'
}

@test "no argument prints the usage" {
    run -1 xrdfs
    assert_output --partial 'Usage:'
}

@test "an unknown option prints the usage" {
    run -1 xrdfs --unknown-option
    assert_output --partial 'Usage:'
}

@test "an endpoint without a host prints the usage" {
    run -1 xrdfs "root://:$SERVER_PORT" stat "$F"
    assert_output --partial 'Usage:'
}

@test "-4 and -6 together are not valid" {
    run -1 xrdfs -6 -4 stat "$URL/$F"
    assert_output 'xrdfs: -4 and -6 are mutually exclusive'
}

@test "a negative debug level is not valid" {
    run -1 xrdfs --debug=-1 stat "$URL/$F"
    assert_output "xrdfs: invalid debug level '-1' (expected 0-3)"
}

@test "debug level 1 is valid" {
    run -0 xrdfs -d 1 stat "$URL/$F"
}

@test "debug level 3 is valid" {
    run -0 xrdfs -d 3 stat "$URL/$F"
}

@test "a command without an endpoint fails" {
    run ! xrdfs query opaque value
}

@test "a local URL as an operand is not valid" {
    run -1 xrdfs "$URL" stat file:///tmp/file
    assert_output 'xrdfs: invalid remote URL operand'
}

#
# interactive mode
#

@test "interactive mode resolves a path relative to the current directory" {
    run bats_pipe -0 printf "mkdir $D/dir\ncd $D/dir\nmkdir child\nstat './child'\nexit\n" \| xrdfs "$URL"
    assert_output --partial "Path:   /t${BATS_TEST_NUMBER}/dir/child"
}

@test "interactive mode resolves a quoted path to the parent directory" {
    run bats_pipe -0 printf "mkdir $D/dir\ncd $D/dir\nstat \"../file\"\nexit\n" \| xrdfs "$URL"
    assert_output --partial "Path:   /t${BATS_TEST_NUMBER}/file"
}

@test "interactive mode refuses a path above the root" {
    run bats_pipe printf "mkdir $D/dir\ncd $D/dir\nstat ../../../escape\nexit\n" \| xrdfs "$URL"
    assert_output --partial 'escapes above root'
}

@test "interactive mode refuses to change to a directory above the root" {
    run bats_pipe printf "cd //\ncd ../../escape\nexit\n" \| xrdfs "$URL"
    assert_output --partial 'escapes above root'
}

@test "interactive help lists the commands" {
    run bats_pipe -0 printf "help\nexit\n" \| xrdfs "$URL"
    assert_output --partial 'Available commands:'
}

@test "interactive exit says goodbye" {
    run bats_pipe -0 printf "exit\n" \| xrdfs "$URL"
    assert_output --partial 'Goodbye.'
}

@test "interactive mode with --no-cwd refuses a relative path" {
    run bats_pipe printf "stat relative\nexit\n" \| xrdfs --no-cwd "$URL"
    assert_output --partial "relative path 'relative' is disallowed"
}

#
# cd
#

@test "cd to a directory succeeds" {
    run -0 xrdfs "$URL" cd "$D"
}

@test "cd to a file fails" {
    run ! xrdfs "$URL" cd "$F"
}

@test "cd to a missing path fails" {
    run ! xrdfs "$URL" cd "$D/missing"
}

@test "cd without a path fails" {
    run ! xrdfs "$URL" cd
}

#
# mkdir and ls
#

@test "mkdir -p with a mode creates the directories" {
    xrdfs "$URL" mkdir -p -mrwxr-x--- "$D/dir/sub"
    run -0 test -d "$SERVER_DATA$D/dir/sub"
}

@test "ls -l -u -D -C lists the URL of an entry" {
    xrdfs "$URL" mkdir -p "$D/dir/sub"
    run -0 xrdfs "$URL" ls -l -u -D -C "$D/dir"
    assert_output --partial "root://localhost:$SERVER_PORT/$D/dir/sub"
}

@test "ls -R -Z -h lists a subdirectory" {
    xrdfs "$URL" mkdir -p "$D/dir/sub"
    run -0 xrdfs "$URL" ls -R -Z -h "$D/dir"
    assert_output --partial 'sub'
}

@test "ls -l of a file lists the file" {
    run -0 xrdfs "$URL" ls -l "$F"
    assert_output --partial "$F"
}

@test "ls -l -h of a file lists the file" {
    run -0 xrdfs "$URL" ls -l -h "$F"
    assert_output --partial "$F"
}

@test "ls -l -h of a directory lists its entries" {
    xrdfs "$URL" mkdir -p "$D/dir/sub"
    run -0 xrdfs "$URL" ls -l -h "$D/dir"
    assert_output --partial 'sub'
}

@test "ls -u of a file lists its URL" {
    run -0 xrdfs "$URL" ls -u "$F"
    assert_output --partial 'root://'
}

@test "ls without a path lists the root" {
    run -0 xrdfs "$URL" ls
}

@test "ls with -R and -Z and -C together is not valid" {
    run ! xrdfs "$URL" ls -l -u -R -D -Z -C "$D"
    assert_output --partial 'Invalid arguments'
}

@test "ls of a relative path is not valid" {
    run ! xrdfs "$URL" ls relative
    assert_output --partial "relative path 'relative' is disallowed"
}

@test "ls of a missing path fails" {
    run ! xrdfs "$URL" ls "$D/missing"
}

@test "ls -Z of a file which is not an archive fails" {
    run ! xrdfs "$URL" ls -Z "$F"
}

@test "mkdir without a path fails" {
    run ! xrdfs "$URL" mkdir
}

@test "mkdir with an invalid mode fails" {
    run ! xrdfs "$URL" mkdir -minvalid "$D/invalid"
}

@test "mkdir of an existing file fails" {
    run ! xrdfs "$URL" mkdir "$F"
}

#
# chmod
#

@test "chmod rwxrwxrwx succeeds" {
    run -0 xrdfs "$URL" chmod "$F" rwxrwxrwx
}

@test "chmod --------- succeeds" {
    run -0 xrdfs "$URL" chmod "$F" ---------
}

@test "chmod with an invalid user permission is not valid" {
    run ! xrdfs "$URL" chmod "$F" q--------
    assert_output --partial 'Invalid arguments'
}

@test "chmod with an invalid group permission is not valid" {
    run ! xrdfs "$URL" chmod "$F" ---q-----
    assert_output --partial 'Invalid arguments'
}

@test "chmod with an invalid other permission is not valid" {
    run ! xrdfs "$URL" chmod "$F" ------q--
    assert_output --partial 'Invalid arguments'
}

@test "chmod with a short mode is not valid" {
    run ! xrdfs "$URL" chmod "$F" short
    assert_output --partial 'Invalid arguments'
}

@test "chmod of a missing file fails" {
    run ! xrdfs "$URL" chmod "$D/missing" rwxr-x---
}

@test "chmod without a mode fails" {
    run ! xrdfs "$URL" chmod "$F"
}

@test "chmod of a relative path fails" {
    run ! xrdfs "$URL" chmod relative rwxr-x---
}

#
# mv, rm and rmdir
#

@test "mv renames a file" {
    xrdfs "$URL" mv "$F" "$D/renamed"
    run -0 test -f "$SERVER_DATA$D/renamed"
}

@test "mv of a directory into itself fails" {
    xrdfs "$URL" mkdir -p "$D/tree/child"
    run ! xrdfs "$URL" mv "$D/tree" "$D/tree/child/moved"
    assert_output --partial 'cannot move directory to a subdirectory of itself'
}

@test "rmdir of a directory which is not empty fails" {
    xrdfs "$URL" mkdir -p "$D/tree/child"
    run ! xrdfs "$URL" rmdir "$D/tree"
}

@test "rm of two files reports each one" {
    echo two > "$SERVER_DATA$D/two"
    run -0 xrdfs "$URL" rm "$F" "$D/two"
    assert_output --partial "rm $F"
    assert_output --partial "rm $D/two"
}

@test "rmdir of an empty directory succeeds" {
    xrdfs "$URL" mkdir "$D/empty"
    run -0 xrdfs "$URL" rmdir "$D/empty"
}

@test "rm without a path fails" {
    run ! xrdfs "$URL" rm
}

@test "rmdir without a path fails" {
    run ! xrdfs "$URL" rmdir
}

@test "mv without a target fails" {
    run ! xrdfs "$URL" mv "$F"
}

@test "mv of a missing file fails" {
    run ! xrdfs "$URL" mv "$D/missing" "$D/destination"
}

@test "mv of a relative source fails" {
    run ! xrdfs "$URL" mv relative "$D/destination"
}

@test "mv to a relative target fails" {
    run ! xrdfs "$URL" mv "$F" relative
}

@test "rmdir of a relative path fails" {
    run ! xrdfs "$URL" rmdir relative
}

@test "rm of a missing file fails" {
    run ! xrdfs "$URL" rm "$D/missing"
}

@test "rm of a relative path fails" {
    run ! xrdfs "$URL" rm relative
}

#
# truncate
#

@test "truncate sets the size" {
    xrdfs "$URL" truncate "$F" 5
    run -0 xrdfs "$URL" stat "$F"
    assert_line --regexp '^Size: +5$'
}

@test "truncate to a size which is not a number is not valid" {
    run ! xrdfs "$URL" truncate "$F" invalid
    assert_output --partial 'Invalid arguments'
}

@test "truncate of a missing file fails" {
    run ! xrdfs "$URL" truncate "$D/missing" 1
}

@test "truncate without a size fails" {
    run ! xrdfs "$URL" truncate "$F"
}

@test "truncate of a relative path fails" {
    run ! xrdfs "$URL" truncate relative 1
}

#
# locate
#

@test "locate -n -r finds the server" {
    run -0 xrdfs "$URL" locate -n -r "$F"
    assert_output --partial 'Server'
}

@test "locate -m -i finds the server" {
    run -0 xrdfs "$URL" locate -m -i "$F"
    assert_output --partial 'Server'
}

@test "locate -d -p finds the server" {
    run -0 xrdfs "$URL" locate -d -p "$F"
    assert_output --partial 'Server'
}

@test "locate of * answers" {
    run xrdfs "$URL" locate '*'
    refute_output ''
}

@test "locate of two paths is not valid" {
    run ! xrdfs "$URL" locate "$F" "$D/other"
    assert_output --partial 'Invalid arguments'
}

@test "locate of a missing path fails" {
    run ! xrdfs "$URL" locate "$D/missing"
}

@test "locate of a relative path fails" {
    run ! xrdfs "$URL" locate relative
}

@test "locate with -n -r -d -m together fails" {
    run ! xrdfs "$URL" locate -n -r -d -m "$F"
}

#
# stat
#

@test "stat of two paths reports the first one" {
    run -0 xrdfs "$URL" stat "$F" "$D"
    assert_output --partial "Path:   $F"
}

@test "stat of two paths reports the second one" {
    run -0 xrdfs "$URL" stat "$F" "$D"
    assert_output --partial "Path:   $D"
}

@test "stat -q of flags which a file has succeeds" {
    run -0 xrdfs "$URL" stat -q 'IsReadable&IsWritable' "$F"
}

@test "stat -q of one of two flags which a directory has succeeds" {
    run -0 xrdfs "$URL" stat -q 'IsDir|Offline' "$D"
}

@test "stat -q of flags which a file has not all fails" {
    run ! xrdfs "$URL" stat -q 'IsDir&IsReadable' "$F"
}

@test "stat -q of flags which a file has none of fails" {
    run ! xrdfs "$URL" stat -q 'Offline|POSCPending' "$F"
}

@test "stat -q of an unknown flag fails" {
    run ! xrdfs "$URL" stat -q Unknown "$F"
}

@test "stat -q without flags fails" {
    run ! xrdfs "$URL" stat -q
}

@test "stat without a path fails" {
    run ! xrdfs "$URL" stat
}

@test "stat of a missing path fails" {
    run ! xrdfs "$URL" stat "$D/missing"
}

#
# statvfs and query
#

@test "statvfs reports the nodes with space" {
    run -0 xrdfs "$URL" statvfs //
    assert_output --partial 'Nodes with RW space:'
}

@test "statvfs of a relative path fails" {
    run ! xrdfs "$URL" statvfs relative
}

@test "statvfs without a path fails" {
    run ! xrdfs "$URL" statvfs
}

@test "query stats a succeeds" {
    run -0 xrdfs "$URL" query stats a
}

# The server need not implement each query; it answers either way, with a
# result or with an error.

@test "query checksum answers" {
    run xrdfs "$URL" query checksum "$F"
    refute_output ''
}

@test "query checksumcancel succeeds" {
    run -0 xrdfs "$URL" query checksumcancel "$F"
}

@test "query opaque answers" {
    run xrdfs "$URL" query opaque "$F"
    refute_output ''
}

@test "query opaquefile answers" {
    run xrdfs "$URL" query opaquefile "$F"
    refute_output ''
}

@test "query space answers" {
    run xrdfs "$URL" query space "$F"
    refute_output ''
}

@test "query xattr answers" {
    run xrdfs "$URL" query xattr "$F"
    refute_output ''
}

@test "query prepare answers" {
    run xrdfs "$URL" query prepare "$F"
    refute_output ''
}

@test "query of an unknown code fails" {
    run ! xrdfs "$URL" query invalid value
}

@test "query config without a parameter fails" {
    run ! xrdfs "$URL" query config
}

@test "query config with two parameters fails" {
    run ! xrdfs "$URL" query config version extra
}

@test "query prepare with a relative path fails" {
    run ! xrdfs "$URL" query prepare request-id "$F" relative
}

@test "query checksum of a relative path fails" {
    run ! xrdfs "$URL" query checksum relative
}

#
# prepare
#

@test "prepare -c -f -s -w -p 3 answers with a request ID" {
    run -0 xrdfs "$URL" prepare -c -f -s -w -p 3 "$F"
    refute_output ''
}

@test "prepare -e succeeds" {
    run -0 xrdfs "$URL" prepare -e "$F"
}

@test "prepare with priority 4 fails" {
    run ! xrdfs "$URL" prepare -p 4 "$F"
}

@test "prepare with a priority which is not a number fails" {
    run ! xrdfs "$URL" prepare -p invalid "$F"
}

@test "prepare -p without a priority fails" {
    run ! xrdfs "$URL" prepare -p
}

@test "prepare -a without a request ID fails" {
    run ! xrdfs "$URL" prepare -a
}

@test "prepare -c without a path fails" {
    run ! xrdfs "$URL" prepare -c
}

@test "prepare without a path fails" {
    run ! xrdfs "$URL" prepare
}

@test "prepare -a of a request ID without a path fails" {
    run ! xrdfs "$URL" prepare -a request-id
}

#
# cat
#

@test "cat of two files prints the first one" {
    echo second > "$SERVER_DATA$D/second"
    run -0 xrdfs "$URL" cat "$F" "$D/second"
    assert_output --partial "$CONTENT"
}

@test "cat of two files prints the second one" {
    echo second > "$SERVER_DATA$D/second"
    run -0 xrdfs "$URL" cat "$F" "$D/second"
    assert_output --partial 'second'
}

@test "cat -o writes the file to a local file" {
    xrdfs "$URL" cat -o "$BATS_TEST_TMPDIR/output" "$F"
    run -0 cmp "$BATS_TEST_TMPDIR/output" "$SERVER_DATA$F"
}

@test "cat -o writes the file to a relative local path" {
    xrdfs "$URL" cat -o output "$F"
    run -0 cmp output "$SERVER_DATA$F"
}

@test "cat -o to a relative path fails without a current directory" {
    mkdir "$BATS_TEST_TMPDIR/gone"
    cd "$BATS_TEST_TMPDIR/gone"
    rmdir "$BATS_TEST_TMPDIR/gone"
    run ! xrdfs "$URL" cat -o output "$F"
    assert_output --partial 'OS Error'
}

@test "cat -o of two files fails" {
    echo second > "$SERVER_DATA$D/second"
    run ! xrdfs "$URL" cat -o "$BATS_TEST_TMPDIR/output" "$F" "$D/second"
}

@test "cat -o without a file name fails" {
    run ! xrdfs "$URL" cat -o
}

@test "cat of a missing file fails" {
    run ! xrdfs "$URL" cat "$D/missing"
}

@test "cat of a relative path fails" {
    run ! xrdfs "$URL" cat relative
}

@test "cat without a path fails" {
    run ! xrdfs "$URL" cat
}

#
# tail
#

@test "tail -c prints the end of the file" {
    run -0 xrdfs "$URL" tail -c 5 "$F"
    assert_output 'cdef'
}

@test "tail -c of more than the size prints the whole file" {
    run -0 xrdfs "$URL" tail -c 100 "$F"
    assert_output "$CONTENT"
}

@test "tail -c of a size which is not a number fails" {
    run ! xrdfs "$URL" tail -c invalid "$F"
}

@test "tail -c without a size fails" {
    run ! xrdfs "$URL" tail -c
}

@test "tail of a missing file fails" {
    run ! xrdfs "$URL" tail "$D/missing"
}

@test "tail -f of a missing file fails" {
    run ! xrdfs "$URL" tail -f "$D/missing"
}

@test "tail of a relative path fails" {
    run ! xrdfs "$URL" tail relative
}

@test "tail without a path fails" {
    run ! xrdfs "$URL" tail
}

@test "tail of a file and an extra argument fails" {
    run ! xrdfs "$URL" tail -c 1 -f "$F" extra
}

#
# spaceinfo and cache
#

@test "spaceinfo reports the largest free chunk" {
    run -0 xrdfs "$URL" spaceinfo //
    assert_output --partial 'Largest free chunk:'
}

@test "spaceinfo without a path fails" {
    run ! xrdfs "$URL" spaceinfo
}

@test "spaceinfo of an unknown space answers" {
    run xrdfs "$URL" spaceinfo invalid-space
    refute_output ''
}

@test "cache with an unknown operation is not valid" {
    run ! xrdfs "$URL" cache invalid "$F"
    assert_output --partial 'Invalid arguments'
}

@test "cache evict on a server which is no cache fails" {
    run ! xrdfs "$URL" cache evict "$F"
}

@test "cache evict without a path fails" {
    run ! xrdfs "$URL" cache evict
}

@test "cache fevict of a relative path fails" {
    run ! xrdfs "$URL" cache fevict relative
}

#
# xattr
#

@test "xattr get returns an attribute which xattr set added" {
    xrdfs "$URL" xattr "$F" set user.key=value
    run -0 xrdfs "$URL" xattr "$F" get user.key
    assert_output --partial 'user.key="value"'
}

@test "xattr list shows an attribute which xattr set added" {
    xrdfs "$URL" xattr "$F" set user.key=value
    run -0 xrdfs "$URL" xattr "$F" list
    assert_output --partial 'user.key="value"'
}

@test "xattr get of an attribute which xattr del removed fails" {
    xrdfs "$URL" xattr "$F" set user.key=value
    xrdfs "$URL" xattr "$F" del user.key
    run ! xrdfs "$URL" xattr "$F" get user.key
}

@test "xattr with an unknown operation fails" {
    run ! xrdfs "$URL" xattr "$F" invalid
}

@test "xattr set without an attribute fails" {
    run ! xrdfs "$URL" xattr "$F" set
}

@test "xattr list with an extra argument fails" {
    run ! xrdfs "$URL" xattr "$F" list extra
}

@test "xattr without an operation fails" {
    run ! xrdfs "$URL" xattr "$F"
}

@test "xattr get without a name fails" {
    run ! xrdfs "$URL" xattr "$F" get
}

@test "xattr del without a name fails" {
    run ! xrdfs "$URL" xattr "$F" del
}

@test "xattr set on a missing file fails" {
    run ! xrdfs "$URL" xattr "$D/missing" set user.key=value
}

@test "xattr del on a missing file fails" {
    run ! xrdfs "$URL" xattr "$D/missing" del user.key
}

@test "xattr list of a missing file fails" {
    run ! xrdfs "$URL" xattr "$D/missing" list
}

@test "xattr list of a relative path fails" {
    run ! xrdfs "$URL" xattr relative list
}
