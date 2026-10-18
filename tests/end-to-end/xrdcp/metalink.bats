#!/usr/bin/env bash

# Copies from metalinks. The client tries the replicas of a metalink in
# order of priority. After an error, it skips all replicas with the host name
# of the failed one, whatever the port, thus the tests use two servers with
# two host names: localhost and 127.0.0.1. Only the server "second" has the
# file /ref.dat.

load ../setup/server.bash

setup_file() {
    start_server first server
    start_server second server

    export REF="$BATS_FILE_TMPDIR/ref.dat"
    make_random_file "$REF" 1000003
    cp "$REF" "$SECOND_DATA/ref.dat"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    METALINK="/t${BATS_TEST_NUMBER}.meta4"
}

# metalink <path> <crc32c or ""> <url...>
#
# Write a metalink for ref.dat with the replicas in order of priority.
metalink() {
    local path=$1 hash=$2 url priority=0
    shift 2

    {
        echo '<?xml version="1.0" encoding="UTF-8"?>'
        echo '<metalink xmlns="urn:ietf:params:xml:ns:metalink">'
        echo '  <file name="ref.dat">'
        echo "    <size>$(wc -c < "$REF")</size>"
        [[ -z "$hash" ]] || echo "    <hash type=\"crc32c\">$hash</hash>"
        for url in "$@"; do
            echo "    <url priority=\"$(( ++priority ))\">$url</url>"
        done
        echo '  </file>'
        echo '</metalink>'
    } > "$path"
}

@test "copy from a metalink on a server gets the file of the replica" {
    metalink "$FIRST_DATA$METALINK" '' "$SECOND_URL4//ref.dat"
    xrdcp "$FIRST_URL/$METALINK" copy.dat
    run -0 cmp "$REF" copy.dat
}

@test "copy from a local metalink gets the file of the replica" {
    metalink local.meta4 '' "$SECOND_URL4//ref.dat"
    xrdcp local.meta4 copy.dat
    run -0 cmp "$REF" copy.dat
}

@test "copy from a metalink tries the next replica when the first has no file" {
    metalink local.meta4 '' "$FIRST_URL//ref.dat" "$SECOND_URL4//ref.dat"
    xrdcp local.meta4 copy.dat
    run -0 cmp "$REF" copy.dat
}

@test "copy from a metalink fails when no replica has the file" {
    metalink local.meta4 '' "$FIRST_URL//ref.dat" "$SECOND_URL4//missing.dat"
    run ! xrdcp local.meta4 copy.dat
    assert_output --partial 'no more replicas to try'
}

@test "--cksum crc32c verifies the hash of the metalink" {
    metalink local.meta4 "$(checksum crc32c "$REF")" "$SECOND_URL4//ref.dat"
    run -0 xrdcp --cksum crc32c local.meta4 copy.dat
}

@test "--cksum crc32c fails when the hash of the metalink is wrong" {
    metalink local.meta4 00000001 "$SECOND_URL4//ref.dat"
    run ! xrdcp --cksum crc32c local.meta4 copy.dat
    assert_output --partial 'CheckSum error'
}
