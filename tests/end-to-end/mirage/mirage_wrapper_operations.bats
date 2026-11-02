#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

# Each test runs one operation of the wrapper twice: once under the mirage path
# (served from memory) and once outside it (forwarded to the disk plugin).
# Operations that no command-line client can trigger are not covered here:
# Ftruncate on an open file, Fsync, ReadV, Reloc and Clone.

readonly MIRAGE="localhost:11974"

setup() {
    PORT=${MIRAGE##*:} launch_xrootd mirage_wrapper.cfg mirage

    sleep 0.5

    # workdir as test tmp dir (all files are removed after execution)
    cd $BATS_TEST_TMPDIR

    printf 'stored on disk' > local.txt
}

teardown() {
    kill_pid_files
}

# --- Create, Open, Write and Close ------------------------------------------

@test "upload outside the mirage path writes the content to disk" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    assert_equal "$(cat mirage/diskfile)" 'stored on disk'
}

@test "upload under the mirage path writes nothing to disk" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    assert [ ! -e mirage/mirage_test ]
}

@test "upload under the mirage path retains the uploaded size" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run -0 xrdfs "root://$MIRAGE" stat /mirage_test/memfile
    assert_output --partial 'Size:   14'
}

@test "overwrite outside the mirage path replaces the content on disk" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"
    printf 'new' > local.txt

    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    assert_equal "$(cat mirage/diskfile)" 'new'
}

@test "overwrite under the mirage path replaces the retained size" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"
    printf 'new' > local.txt

    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run -0 xrdfs "root://$MIRAGE" stat /mirage_test/memfile
    assert_output --partial 'Size:   3'
}

@test "upload to a sibling of the mirage path is stored on disk" {
    run -0 xrdcp -f -p local.txt "root://$MIRAGE//mirage_test2/file"

    assert_equal "$(cat mirage/mirage_test2/file)" 'stored on disk'
}

# --- Open, Read and Fstat ---------------------------------------------------

@test "download outside the mirage path returns the stored content" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    run -0 xrdcp -f "root://$MIRAGE//diskfile" -
    assert_output 'stored on disk'
}

@test "download under the mirage path returns the default pattern" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run -0 xrdcp -f "root://$MIRAGE//mirage_test/memfile" -
    assert_output '00000000000000'
}

@test "download of a missing file outside the mirage path fails" {
    run ! xrdcp -f "root://$MIRAGE//missing" downloaded.txt
    assert_output --partial 'no such file or directory'
}

@test "download of a missing file under the mirage path fails" {
    run ! xrdcp -f "root://$MIRAGE//mirage_test/missing" downloaded.txt
    assert_output --partial 'no such file or directory'
}

# --- Stat -------------------------------------------------------------------

@test "stat outside the mirage path reports the size on disk" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    run -0 xrdfs "root://$MIRAGE" stat /diskfile
    assert_output --partial 'Size:   14'
}

@test "stat of a missing file outside the mirage path fails" {
    run ! xrdfs "root://$MIRAGE" stat /missing
    assert_output --partial 'no such file or directory'
}

@test "stat of a missing file under the mirage path fails" {
    run ! xrdfs "root://$MIRAGE" stat /mirage_test/missing
    assert_output --partial 'no such file or directory'
}

# --- Truncate ---------------------------------------------------------------

@test "truncate outside the mirage path changes the size on disk" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    run -0 xrdfs "root://$MIRAGE" truncate /diskfile 100

    assert_equal "$(wc -c < mirage/diskfile | tr -d ' ')" 100
}

@test "truncate under the mirage path changes the retained size" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run -0 xrdfs "root://$MIRAGE" truncate /mirage_test/memfile 100

    run -0 xrdfs "root://$MIRAGE" stat /mirage_test/memfile
    assert_output --partial 'Size:   100'
}

@test "truncate of a missing file outside the mirage path fails" {
    run ! xrdfs "root://$MIRAGE" truncate /missing 100
    assert_output --partial 'no such file or directory'
}

@test "truncate of a missing file under the mirage path fails" {
    run ! xrdfs "root://$MIRAGE" truncate /mirage_test/missing 100
    assert_output --partial 'no such file or directory'
}

# --- Unlink -----------------------------------------------------------------

@test "removal outside the mirage path deletes the file on disk" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    run -0 xrdfs "root://$MIRAGE" rm /diskfile

    assert [ ! -e mirage/diskfile ]
}

@test "removal under the mirage path deletes the retained file" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run -0 xrdfs "root://$MIRAGE" rm /mirage_test/memfile

    run ! xrdfs "root://$MIRAGE" stat /mirage_test/memfile
    assert_output --partial 'no such file or directory'
}

@test "removal of a missing file outside the mirage path fails" {
    run ! xrdfs "root://$MIRAGE" rm /missing
    assert_output --partial 'no such file or directory'
}

@test "removal of a missing file under the mirage path fails" {
    run ! xrdfs "root://$MIRAGE" rm /mirage_test/missing
    assert_output --partial 'no such file or directory'
}

# --- Rename -----------------------------------------------------------------

@test "rename outside the mirage path moves the file on disk" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    run -0 xrdfs "root://$MIRAGE" mv /diskfile /renamed

    assert_equal "$(cat mirage/renamed)" 'stored on disk'
    assert [ ! -e mirage/diskfile ]
}

@test "rename under the mirage path keeps the retained size at the new name" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run -0 xrdfs "root://$MIRAGE" mv /mirage_test/memfile /mirage_test/renamed

    run -0 xrdfs "root://$MIRAGE" stat /mirage_test/renamed
    assert_output --partial 'Size:   14'
}

@test "rename under the mirage path removes the old name" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run -0 xrdfs "root://$MIRAGE" mv /mirage_test/memfile /mirage_test/renamed

    run ! xrdfs "root://$MIRAGE" stat /mirage_test/memfile
    assert_output --partial 'no such file or directory'
}

@test "rename from the disk path to the mirage path fails" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    run ! xrdfs "root://$MIRAGE" mv /diskfile /mirage_test/memfile
    assert_output --partial 'cross-device link'
}

@test "rename from the mirage path to the disk path fails" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run ! xrdfs "root://$MIRAGE" mv /mirage_test/memfile /diskfile
    assert_output --partial 'cross-device link'
}

@test "rename from the mirage path to the disk path keeps the source file" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run ! xrdfs "root://$MIRAGE" mv /mirage_test/memfile /diskfile

    run -0 xrdfs "root://$MIRAGE" stat /mirage_test/memfile
    assert_output --partial 'Size:   14'
}

# --- Chmod ------------------------------------------------------------------

@test "chmod outside the mirage path changes the mode on disk" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    run -0 xrdfs "root://$MIRAGE" chmod /diskfile rwxr-x---

    assert_equal "$(ls -l mirage/diskfile | cut -c2-10)" 'rwxr-x---'
}

@test "chmod under the mirage path is not supported" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run ! xrdfs "root://$MIRAGE" chmod /mirage_test/memfile rwxr-x---
    assert_output --partial 'not supported'
}

# --- Mkdir and Remdir -------------------------------------------------------

@test "mkdir outside the mirage path creates a directory on disk" {
    run -0 xrdfs "root://$MIRAGE" mkdir /diskdir

    assert [ -d mirage/diskdir ]
}

@test "mkdir under the mirage path is not supported" {
    run ! xrdfs "root://$MIRAGE" mkdir /mirage_test/memdir
    assert_output --partial 'not supported'
}

@test "rmdir outside the mirage path removes the directory on disk" {
    run -0 xrdfs "root://$MIRAGE" mkdir /diskdir

    run -0 xrdfs "root://$MIRAGE" rmdir /diskdir

    assert [ ! -e mirage/diskdir ]
}

@test "rmdir under the mirage path is not supported" {
    run ! xrdfs "root://$MIRAGE" rmdir /mirage_test/memdir
    assert_output --partial 'not supported'
}

# --- Opendir, Readdir and Close of a directory ------------------------------

@test "directory listing outside the mirage path shows the files on disk" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"
    run -0 xrdfs "root://$MIRAGE" mkdir /diskdir
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskdir/inner"

    run -0 xrdfs "root://$MIRAGE" ls /diskdir
    assert_output '/diskdir/inner'
}

@test "directory listing of a missing directory outside the mirage path fails" {
    run ! xrdfs "root://$MIRAGE" ls /missingdir
    assert_output --partial 'no such file or directory'
}

@test "directory listing under the mirage path fails" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run ! xrdfs "root://$MIRAGE" ls /mirage_test
    assert_output --partial '/mirage_test'
}

# --- StatLS, StatXA and StatXP ----------------------------------------------

@test "space query outside the mirage path reports the disk space" {
    run -0 xrdfs "root://$MIRAGE" query space /
    assert_output --partial 'oss.space='
}

@test "space query under the mirage path is not supported" {
    run ! xrdfs "root://$MIRAGE" query space /mirage_test/memfile
    assert_output --partial 'not supported'
}

@test "attribute query outside the mirage path reports the file type" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    run -0 xrdfs "root://$MIRAGE" query xattr /diskfile
    assert_output --partial 'oss.type=f'
}

@test "attribute query under the mirage path is not supported" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run ! xrdfs "root://$MIRAGE" query xattr /mirage_test/memfile
    assert_output --partial 'not supported'
}

@test "locate outside the mirage path returns the server" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//diskfile"

    run -0 xrdfs "root://$MIRAGE" locate /diskfile
    assert_output --partial 'Server ReadWrite'
}

@test "locate under the mirage path returns the server" {
    run -0 xrdcp -f local.txt "root://$MIRAGE//mirage_test/memfile"

    run -0 xrdfs "root://$MIRAGE" locate /mirage_test/memfile
    assert_output --partial 'Server'
}
