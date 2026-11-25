#!/usr/bin/env bash

load ../setup/server.bash

setup() {
    require_commands jq python3
    start_server server server-nosec
    endpoint=$SERVER_URL
    echo '0123456789abcdef' > "$SERVER_DATA/file"
}

teardown() {
    stop_servers
}

@test "JSON stat retains metadata and per-path failures" {
    run --separate-stderr xrdfs stat --json \
        "$endpoint//file?authz=fixture" relative "$endpoint//missing" "$endpoint//file"
    assert_failure
    jq -Ren '
        [inputs | fromjson] |
        length == 4 and
        (.[0] | .version == 1 and .status.ok and (.status.partial | not) and
         (.entries[0] | .path == "/file" and .size == 17 and
          .extended and (.atime | type) == "number" and (.ctime | type) == "number" and
          (.flags | type) == "number" and (.flag_names | type) == "array")) and
        (.[1] | (.status.ok | not) and .status.code == 9 and .entries == []) and
        (.[2] | (.status.ok | not) and .entries == []) and
        (.[3] | .status.ok and .entries[0].path == "/file")
    ' <<< "$output"

    run --separate-stderr xrdfs "$endpoint" stat --json relative ''
    assert_failure
    jq -Ren '
        [inputs | fromjson] |
        length == 2 and all(.[]; (.status.ok | not) and .status.code == 9 and .entries == [])
    ' <<< "$output"
}

@test "JSON stat rejects an empty -q argument" {
    run --separate-stderr xrdfs "$endpoint" stat --json -q '' /file
    assert_failure
    assert_output ''

    run -0 xrdfs "$endpoint" stat -q '' /file
    assert_output --partial 'Path:   /file'
}

@test "JSON stat reports signed timestamps before the epoch" {
    local path="$SERVER_DATA/file"
    python3 -c 'import os, sys; os.utime(sys.argv[1], (-1, -1))' "$path"

    run -0 xrdfs stat --json "$endpoint//file"
    jq -e '.entries[0] | .extended and .mtime == -1 and .atime == -1' <<< "$output"

    run -0 xrdfs "file://$BATS_TEST_TMPDIR" stat --json "$path"
    jq -e '.entries[0] | .mtime == -1 and .atime == null and .ctime == null' <<< "$output"
}

@test "JSON listing identifies directories and empty results" {
    run -0 xrdfs "$endpoint" mkdir //empty
    run -0 xrdfs ls --json "$endpoint//"
    jq -es '
        length == 1 and
        (.[0] | .version == 1 and .status.ok and (.status.partial | not) and
         any(.entries[]; .path == "/file" and .type == "file") and
         any(.entries[]; .path == "/empty" and .type == "directory"))
    ' <<< "$output"

    run -0 xrdfs ls --json "$endpoint//empty"
    jq -es '
        length == 1 and (.[0] | .status.ok and (.status.partial | not) and .entries == [])
    ' <<< "$output"
}

@test "JSON listing validates options and the path" {
    run -0 xrdfs "$endpoint" mkdir //empty
    run -0 xrdfs ls --json -l -R -D -C -h "$endpoint//empty"
    jq -es 'length == 1 and .[0].status.ok and .[0].entries == []' <<< "$output"

    for option in -u -Z --unknown; do
        run --separate-stderr xrdfs "$endpoint" ls "$option" --json /file
        assert_failure
        jq -es 'length == 1 and (.[0] | (.status.ok | not) and .entries == [])' <<< "$output"
    done

    run --separate-stderr xrdfs "$endpoint" ls /file /empty --json
    assert_failure
    jq -es 'length == 1 and (.[0] | (.status.ok | not) and .entries == [])' <<< "$output"

    run --separate-stderr xrdfs "$endpoint" ls --json
    assert_failure
    jq -es 'length == 1 and (.[0] | (.status.ok | not) and .entries == [])' <<< "$output"
}

@test "JSON metadata preserves literal question marks in local filenames" {
    local path="$BATS_TEST_TMPDIR/report?final"
    printf 'local' > "$path"

    for command in stat ls; do
        run -0 xrdfs "file://$BATS_TEST_TMPDIR" "$command" --json "$path"
        jq -es --arg path "$path" '
            length == 1 and (.[0] | .status.ok and
             (.entries[0] | .path == $path and (.extended | not) and
              has("atime") and .atime == null and has("ctime") and .ctime == null))
        ' <<< "$output"
    done
}

@test "JSON listing keeps valid entries in a directory with non-UTF-8 filenames" {
    run -0 xrdfs "$endpoint" mkdir /encoding
    local directory="$SERVER_DATA/encoding"
    touch "$directory/good"
    touch "$directory/"$'bad\xff' 2>/dev/null || skip "filesystem rejects non-UTF-8 filenames"

    run -0 xrdfs ls --json "$endpoint//encoding"
    jq -es '
        length == 1 and
        (.[0] | .status.ok and .status.partial and [.entries[].path] == ["/encoding/good"])
    ' <<< "$output"
}

@test "JSON listing omits remote names containing literal question marks" {
    run -0 xrdfs "$endpoint" mkdir /encoding
    local directory="$SERVER_DATA/encoding"
    touch "$directory/report" "$directory/report?final" "$directory/report%3Ffinal"

    run -0 xrdfs ls --json "$endpoint//encoding"
    jq -es '
        length == 1 and
        (.[0] | .status.ok and .status.partial and
         ([.entries[].path] | sort) == ["/encoding/report", "/encoding/report%3Ffinal"])
    ' <<< "$output"
}
