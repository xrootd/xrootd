#!/usr/bin/env bash

# Namespace operations on a file. The tests share one server, and each test
# has its own file on it.

load ../setup/server.bash

# 128 bytes, thus truncate and tail have data to work on
CONTENT=$(printf 'abcdefghijklmnop%.0s' {1..8})
readonly CONTENT

setup_file() {
    start_server server server
}

teardown_file() {
    stop_servers
}

setup() {
    FILE="/t${BATS_TEST_NUMBER}.dat"
    printf '%s' "$CONTENT" | xrdcp - "$SERVER_URL/$FILE"
}

@test "stat reports the size of the file" {
    run -0 xrdfs "$SERVER_URL" stat "$FILE"
    assert_line --regexp '^Size: +128$'
}

@test "xattr list shows the file" {
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" list
    assert_output --partial "# file: $FILE"
}

@test "xattr list shows an attribute which xattr set added" {
    require_xattr "$SERVER_DATA"
    xrdfs "$SERVER_URL" xattr "$FILE" set color=blue
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" list
    assert_line 'color="blue"'
}

@test "xattr list does not show the checksum which the server stores" {
    # the server stores checksums in extended attributes
    require_xattr "$SERVER_DATA"
    xrdfs "$SERVER_URL" query checksum "$FILE?cks.type=adler32"
    run -0 xrdfs "$SERVER_URL" xattr "$FILE" list
    refute_output --partial 'adler32'
}

@test "stat of a path with repeated slashes finds the file" {
    run -0 xrdfs "$SERVER_URL" stat "//./$FILE"
    assert_line --regexp '^Size: +128$'
}

@test "chmod rwxr-xr-x sets mode 0755" {
    xrdfs "$SERVER_URL" chmod "$FILE" rwxr-xr-x
    run -0 xrdfs "$SERVER_URL" stat "$FILE"
    assert_line --regexp '^Mode: +0755$'
}

@test "chmod rwxrwxr-x sets mode 0775" {
    xrdfs "$SERVER_URL" chmod "$FILE" rwxrwxr-x
    run -0 xrdfs "$SERVER_URL" stat "$FILE"
    assert_line --regexp '^Mode: +0775$'
}

@test "chmod rw-r--r-- sets mode 0644" {
    xrdfs "$SERVER_URL" chmod "$FILE" rwxrwxrwx
    xrdfs "$SERVER_URL" chmod "$FILE" rw-r--r--
    run -0 xrdfs "$SERVER_URL" stat "$FILE"
    assert_line --regexp '^Mode: +0644$'
}

@test "ls shows the path of the file" {
    run -0 xrdfs "$SERVER_URL" ls "$FILE"
    assert_output "$FILE"
}

@test "ls -u shows the URL of the file" {
    run -0 xrdfs "$SERVER_URL" ls -u "$FILE"
    assert_output "$SERVER_URL/$FILE"
}

@test "ls -l shows the size of the file" {
    run -0 xrdfs "$SERVER_URL" ls -l "$FILE"
    assert_output --regexp " 128 .* $FILE\$"
}

@test "ls -l of a directory shows the size of its file" {
    xrdfs "$SERVER_URL" mkdir "/d${BATS_TEST_NUMBER}"
    xrdfs "$SERVER_URL" mv "$FILE" "/d${BATS_TEST_NUMBER}$FILE"
    run -0 xrdfs "$SERVER_URL" ls -l "/d${BATS_TEST_NUMBER}"
    assert_output --regexp " 128 .* /d${BATS_TEST_NUMBER}$FILE\$"
}

@test "ls -D shows the path of the file" {
    run -0 xrdfs "$SERVER_URL" ls -D "$FILE"
    assert_output "$FILE"
}

@test "ls -R of the root shows the file" {
    run -0 xrdfs "$SERVER_URL" ls -R /
    assert_line "${FILE#/}"
}

@test "ls -R of the root shows a file in a subdirectory" {
    xrdfs "$SERVER_URL" mkdir -p "/r${BATS_TEST_NUMBER}/sub"
    xrdfs "$SERVER_URL" mv "$FILE" "/r${BATS_TEST_NUMBER}/sub$FILE"
    run -0 xrdfs "$SERVER_URL" ls -R /
    assert_line "r${BATS_TEST_NUMBER}/sub$FILE"
}

@test "locate -r finds the file on the server" {
    run -0 xrdfs "$SERVER_URL" locate -r "$FILE"
    assert_output --partial ":$SERVER_PORT Server ReadWrite"
}

@test "locate -n finds the file on the server" {
    run -0 xrdfs "$SERVER_URL" locate -n "$FILE"
    assert_output --partial ":$SERVER_PORT Server ReadWrite"
}

@test "locate -d finds the file on the server" {
    run -0 xrdfs "$SERVER_URL" locate -d "$FILE"
    assert_output --partial ":$SERVER_PORT Server ReadWrite"
}

@test "locate -m finds the file on the server" {
    run -0 xrdfs "$SERVER_URL" locate -m "$FILE"
    assert_output --partial ":$SERVER_PORT Server ReadWrite"
}

@test "locate -i finds the file on the server" {
    run -0 xrdfs "$SERVER_URL" locate -i "$FILE"
    assert_output --partial ":$SERVER_PORT Server ReadWrite"
}

@test "locate -p finds the file on the server" {
    run -0 xrdfs "$SERVER_URL" locate -p "$FILE"
    assert_output --partial ":$SERVER_PORT Server ReadWrite"
}

@test "truncate sets the size of the file" {
    xrdfs "$SERVER_URL" truncate "$FILE" 64
    run -0 xrdfs "$SERVER_URL" stat "$FILE"
    assert_line --regexp '^Size: +64$'
}

@test "cat prints the content of the file" {
    run -0 xrdfs "$SERVER_URL" cat "$FILE"
    assert_output "$CONTENT"
}

@test "tail -c prints the end of the file" {
    run -0 xrdfs "$SERVER_URL" tail -c 32 "$FILE"
    assert_output "${CONTENT: -32}"
}

@test "rm removes the file" {
    xrdfs "$SERVER_URL" rm "$FILE"
    run ! xrdfs "$SERVER_URL" stat "$FILE"
    assert_output --partial '[3011]'
}
