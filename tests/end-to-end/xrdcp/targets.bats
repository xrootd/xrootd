#!/usr/bin/env bash

# The target of xrdcp: a file, a directory, or the top of the server. Each
# test has its own server.

load ../setup/server.bash

setup() {
    start_server
    cd "$BATS_TEST_TMPDIR"
    echo 'content' > file.txt
}

teardown() {
    stop_servers
}

# Only the files which a recursive copy finds in a local directory make the
# target a directory. A file whose name is as long as its directory part,
# slash included, passes a check of the length of the path alone.
@test "a recursive copy of one file whose name is as long as its directory writes the target file" {
    local name
    name=$(printf 'f%.0s' $(seq $(( ${#PWD} + 1 ))))
    echo 'content' > "$name"
    xrdcp -r "$PWD/$name" "$SERVER_URL//out"
    run -0 test -f "$SERVER_DATA/out"
}

@test "a recursive copy of a local directory writes its files below the target" {
    mkdir -p dir/sub
    echo 'content' > dir/sub/file
    xrdcp -r "$PWD/dir" "$SERVER_URL//out"
    run -0 test -f "$SERVER_DATA/out/dir/sub/file"
}

@test "a copy to a URL without a path fails with invalid arguments" {
    run -50 xrdcp file.txt "root://localhost:$SERVER_PORT"
    assert_output --partial 'Invalid arguments'
}

@test "a copy to an existing file without --force fails with file exists" {
    echo 'old' > "$SERVER_DATA/out"
    run -50 xrdcp file.txt "$SERVER_URL//out"
    assert_output --partial 'Run: [ERROR] Invalid operation'
}

@test "a copy with --continue to an existing file writes the rest of the source" {
    printf 'con' > "$SERVER_DATA/out"
    xrdcp --continue file.txt "$SERVER_URL//out"
    run -0 cmp file.txt "$SERVER_DATA/out"
}

@test "a copy to an existing file with --force replaces it" {
    echo 'old' > "$SERVER_DATA/out"
    xrdcp --force file.txt "$SERVER_URL//out"
    run -0 cmp file.txt "$SERVER_DATA/out"
}

@test "a copy into the top of the server writes the file with the name of the source" {
    xrdcp file.txt "root://localhost:$SERVER_PORT/"
    run -0 cmp file.txt "$SERVER_DATA/file.txt"
}
