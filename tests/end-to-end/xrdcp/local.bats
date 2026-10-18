#!/usr/bin/env bash

# Copies of local files, which use no server.

load ../setup/server.bash

setup() {
    cd "$BATS_TEST_TMPDIR"
    make_random_file local.dat 100003
}

@test "copy of a local file keeps the content" {
    xrdcp local.dat copy.dat
    run -0 cmp local.dat copy.dat
}

@test "copy with --path creates the missing parent directories" {
    xrdcp --path local.dat a/b/c/copy.dat
    run -0 cmp local.dat a/b/c/copy.dat
}

@test "copy of a local file which does not exist fails" {
    run ! xrdcp missing.dat copy.dat
    assert_output --partial 'no such file or directory'
}

@test "copy to an existing file without --force fails" {
    echo 'old' > copy.dat
    run ! xrdcp local.dat copy.dat
}

@test "copy to an existing file with --force replaces it" {
    echo 'old' > copy.dat
    xrdcp --force local.dat copy.dat
    run -0 cmp local.dat copy.dat
}

@test "copy to a local directory writes the file with the name of the source into it" {
    mkdir dir
    xrdcp local.dat dir
    run -0 cmp local.dat dir/local.dat
}
