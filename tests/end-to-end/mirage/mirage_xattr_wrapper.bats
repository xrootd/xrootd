#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

setup() {
    launch_xrootd mirage_xattr_wrapper.cfg mirage

    sleep 0.5

    # workdir as test tmp dir (all files are removed after execution)
    cd $BATS_TEST_TMPDIR

    printf 'stored on disk' > local.txt

    # a small placeholder upload; the mirage plugin only retains the size
    head -c 100 /dev/zero > zeros.bin
}

teardown() {
    kill_pid_files
}

bats::on_failure() {
    print_log_files
}

# --- attributes under the mirage path are served by mirage ------------------

@test "an extended attribute set under the mirage path can be read back" {
    run -0 xrdcp -f zeros.bin root://localhost:6543//mirage_test/memfile

    run -0 xrdfs root://localhost:6543/ xattr /mirage_test/memfile set pattern=abcde

    run -0 xrdfs root://localhost:6543/ xattr /mirage_test/memfile get pattern
    assert_output --partial 'pattern="abcde"'
}

@test "the pattern attribute determines the content served under the mirage path" {
    run -0 xrdcp -f zeros.bin root://localhost:6543//mirage_test/memfile

    run -0 xrdfs root://localhost:6543/ xattr /mirage_test/memfile set pattern=abcde

    # the 100 byte file is filled with the repeated pattern
    run -0 xrdfs root://localhost:6543/ cat /mirage_test/memfile
    assert_output --partial 'abcdeabcdeabcde'
}

@test "a custom open return code makes opening a file under the mirage path fail" {
    run -0 xrdcp -f zeros.bin root://localhost:6543//mirage_test/memfile

    # errno 12 (ENOMEM) surfaces to the client as 'cannot allocate memory'
    run -0 xrdfs root://localhost:6543/ xattr /mirage_test/memfile set open.return_code=12

    run ! xrdcp -f root://localhost:6543//mirage_test/memfile downloaded.bin
    assert_output --partial 'Unable to open'
}

@test "deleting the open return code restores normal opening under the mirage path" {
    run -0 xrdcp -f zeros.bin root://localhost:6543//mirage_test/memfile
    run -0 xrdfs root://localhost:6543/ xattr /mirage_test/memfile set open.return_code=12

    run ! xrdcp -f root://localhost:6543//mirage_test/memfile blocked.bin

    run -0 xrdfs root://localhost:6543/ xattr /mirage_test/memfile del open.return_code
    run -0 xrdcp -f root://localhost:6543//mirage_test/memfile allowed.bin
}

# --- attributes outside the mirage path are forwarded to the stacked plugin -

@test "a mirage attribute set outside the mirage path does not change the served content" {
    run -0 xrdcp -f local.txt root://localhost:6543//diskfile

    # routed to the wrapped plugin, 'pattern' is stored as an ordinary attribute
    # and has none of the special meaning it carries under the mirage path
    run -0 xrdfs root://localhost:6543/ xattr /diskfile set pattern=AAAA

    run -0 xrdfs root://localhost:6543/ cat /diskfile
    assert_output --partial 'stored on disk'
    refute_output --partial 'AAAA'
}

@test "mirage return codes set on a real file do not inject any error" {
    run -0 xrdcp -f local.txt root://localhost:6543//diskfile

    # routed to the wrapped plugin, the return codes are stored as ordinary
    # attributes and carry none of the error-injecting meaning they have under
    # the mirage path
    run -0 xrdfs root://localhost:6543/ xattr /diskfile set open.return_code=12
    run -0 xrdfs root://localhost:6543/ xattr /diskfile set read.return_code=12
    run -0 xrdfs root://localhost:6543/ xattr /diskfile set write.return_code=12

    # the real file is still opened and read back normally, with its real content
    run -0 xrdcp -f root://localhost:6543//diskfile downloaded.txt
    assert_equal "$(cat downloaded.txt)" 'stored on disk'

    # and it can still be written (overwritten) normally
    run -0 xrdcp -f local.txt root://localhost:6543//diskfile
}

@test "setting a mirage return code on a non-existent real path fails" {
    # a real (non-mirage) path is forwarded to the wrapped plugin, which has no
    # file to attach the attribute to and reports the failure
    run ! xrdfs root://localhost:6543/ xattr /doesnotexist set open.return_code=12
    assert_output --partial 'error'
}
