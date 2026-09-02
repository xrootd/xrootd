#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

setup() {
    export XRDFS=${XRDFS:-xrdfs}
    umask 022
    mkdir -p "$BATS_TEST_TMPDIR/xrdfs-full-url/data/subdir"
    printf 'first' > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/first.txt"
    printf 'second' > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/second.txt"
    : > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/empty.txt"
    printf 'hidden' > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/.hidden.txt"
    printf 'nested' \
        > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/subdir/nested.txt"
    dd if=/dev/zero \
        of="$BATS_TEST_TMPDIR/xrdfs-full-url/data/1025-bytes.dat" \
        bs=1025 count=1 2>/dev/null
    printf '\000\001\177\200\377' \
        > "$BATS_TEST_TMPDIR/xrdfs-full-url/data/binary.dat"
    printf 'dash' > "$BATS_TEST_TMPDIR/xrdfs-full-url/-dash.txt"
    ln -s data/first.txt "$BATS_TEST_TMPDIR/xrdfs-full-url/remove-link"

    launch_xrootd xrdfs-full-url.cfg xrdfs-full-url

    export TEST_ENDPOINT=root://localhost:11965
    export TEST_DIRECTORY=$TEST_ENDPOINT//data/
    export TEST_SUBDIRECTORY=$TEST_ENDPOINT//data/subdir/
    export TEST_FILE=$TEST_ENDPOINT//data/first.txt
    export TEST_SECOND_FILE=$TEST_ENDPOINT//data/second.txt
    export TEST_EMPTY_FILE=$TEST_ENDPOINT//data/empty.txt
    export TEST_BINARY_FILE=$TEST_ENDPOINT//data/binary.dat

    local ready=false
    for _ in {1..50}; do
        if "$XRDFS" "$TEST_ENDPOINT" stat /data/first.txt \
            >/dev/null 2>&1; then
            ready=true
            break
        fi
        sleep 0.1
    done
    "$ready"

    # Seed one attribute on the ephemeral local fixture. The commands under
    # test only use the read-only list/get forms.
    "$XRDFS" "$TEST_ENDPOINT" xattr /data/first.txt set \
        user.test=fixture >/dev/null
}

teardown() {
    # The server can exit between the last command and teardown. Cleanup must
    # remain idempotent when a recorded PID has already disappeared.
    kill_pid_files 2>/dev/null || true
}

bats::on_failure() {
    print_log_files
}

local_mode() {
    if stat -c '%a' "$1" >/dev/null 2>&1; then
        stat -c '%a' "$1"
    else
        stat -f '%Lp' "$1"
    fi
}

request_count() {
    awk -v method="$1" -v path="$2" '
        $1 == method && $2 == path { count++ }
        END { print count + 0 }
    ' "$3"
}

request_query_count() {
    awk -v method="$1" -v path="$2" -v query="$3" '
        $1 == method && $2 == path && index($3, query) { count++ }
        END { print count + 0 }
    ' "$4"
}

@test "legacy and complete-URL stat forms have identical output" {
    run "$XRDFS" "$TEST_ENDPOINT" stat /data/first.txt
    assert_success
    local legacy_output=$output

    run "$XRDFS" stat "$TEST_FILE"
    assert_success
    assert_output "$legacy_output"
    assert_output --partial 'Size:   5'
}

@test "legacy and complete-URL ls forms have identical output" {
    run "$XRDFS" "$TEST_ENDPOINT" ls -l /data/
    assert_success
    local legacy_output=$output

    run "$XRDFS" ls -l "$TEST_DIRECTORY"
    assert_success
    assert_output "$legacy_output"
    assert_output --partial first.txt
}

@test "mkdir accepts gfal octal modes and multiple complete URLs" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url

    run "$XRDFS" mkdir -p -m 0751 \
        "$TEST_ENDPOINT//mkdir-compat/separate-one" \
        "$TEST_ENDPOINT//mkdir-compat/separate-two"
    assert_success
    run local_mode "$root/mkdir-compat/separate-one"
    assert_success
    assert_output 751
    run local_mode "$root/mkdir-compat/separate-two"
    assert_success
    assert_output 751

    run "$XRDFS" mkdir --parents --mode=0710 \
        "$TEST_ENDPOINT//mkdir-compat/long-equals"
    assert_success
    run local_mode "$root/mkdir-compat/long-equals"
    assert_success
    assert_output 710

    run "$XRDFS" mkdir -p -m0701 \
        "$TEST_ENDPOINT//mkdir-compat/short-attached"
    assert_success
    run local_mode "$root/mkdir-compat/short-attached"
    assert_success
    assert_output 701

    run "$XRDFS" mkdir -p --mode 0711 \
        "$TEST_ENDPOINT//mkdir-compat/long-separated"
    assert_success
    run local_mode "$root/mkdir-compat/long-separated"
    assert_success
    assert_output 711
}

@test "mkdir preserves legacy symbolic modes and the default mode" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url

    run "$XRDFS" "$TEST_ENDPOINT" mkdir -p -mrwxr-x--- \
        /mkdir-compat/legacy
    assert_success
    run local_mode "$root/mkdir-compat/legacy"
    assert_success
    assert_output 750

    run "$XRDFS" mkdir -p "$TEST_ENDPOINT//mkdir-compat/default"
    assert_success
    run local_mode "$root/mkdir-compat/default"
    assert_success
    assert_output 750
}

@test "mkdir validates every operand before the first mutation" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url

    for mode in 0788 1000 0755garbage ''; do
        run "$XRDFS" mkdir --mode="$mode" \
            "$TEST_ENDPOINT//invalid-mode-one" \
            "$TEST_ENDPOINT//invalid-mode-two"
        assert_failure
        run test ! -e "$root/invalid-mode-one"
        assert_success
        run test ! -e "$root/invalid-mode-two"
        assert_success

        run "$XRDFS" mkdir --mode="$mode" --mode=0755 \
            "$TEST_ENDPOINT//overwritten-invalid-mode"
        assert_failure
        run test ! -e "$root/overwritten-invalid-mode"
        assert_success
    done

    run "$XRDFS" "$TEST_ENDPOINT" mkdir /valid-first-path ''
    assert_failure
    run test ! -e "$root/valid-first-path"
    assert_success

    run "$XRDFS" mkdir "$TEST_ENDPOINT//same-endpoint-first" \
        root://127.0.0.1:11965//different-endpoint-second
    assert_failure 1
    assert_output 'xrdfs: all URL operands must use the same endpoint'
    run test ! -e "$root/same-endpoint-first"
    assert_success
}

@test "mkdir option delimiter preserves a dash-prefixed path" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url

    run bash -c \
        'printf "cd /\nmkdir -- -mkdir-compat\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    run test -d "$root/-mkdir-compat"
    assert_success

    run "$XRDFS" "$TEST_ENDPOINT" mkdir -rejected-without-delimiter
    assert_failure
    run test ! -e "$root/-rejected-without-delimiter"
    assert_success
}

@test "chmod accepts gfal mode-first and legacy path-first forms" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    local url=$TEST_ENDPOINT//chmod-compat

    run "$XRDFS" mkdir "$url"
    assert_success

    run "$XRDFS" chmod 0715 "$url"
    assert_success
    run local_mode "$root/chmod-compat"
    assert_success
    assert_output 715

    run "$XRDFS" chmod "$url" rwxr-x---
    assert_success
    run local_mode "$root/chmod-compat"
    assert_success
    assert_output 750

    run "$XRDFS" "$TEST_ENDPOINT" chmod /chmod-compat 0704
    assert_success
    run local_mode "$root/chmod-compat"
    assert_success
    assert_output 704
}

@test "chmod keeps path-first precedence for mode-shaped path names" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url

    run "$XRDFS" mkdir "$TEST_ENDPOINT//0755"
    assert_success
    run bash -c \
        'printf "cd /\nchmod 0755 0710\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    run local_mode "$root/0755"
    assert_success
    assert_output 710
}

@test "mv accepts complete URLs and preserves legacy syntax" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url

    printf 'complete-url' > "$root/mv-url-source"
    run "$XRDFS" mv \
        "$TEST_ENDPOINT//mv-url-source" \
        "$TEST_ENDPOINT//mv-url-destination"
    assert_success
    run test ! -e "$root/mv-url-source"
    assert_success
    run test "$(cat "$root/mv-url-destination")" = complete-url
    assert_success

    printf 'legacy' > "$root/mv-legacy-source"
    run "$XRDFS" "$TEST_ENDPOINT" mv \
        /mv-legacy-source /mv-legacy-destination
    assert_success
    run test ! -e "$root/mv-legacy-source"
    assert_success
    run test "$(cat "$root/mv-legacy-destination")" = legacy
    assert_success
}

@test "mv replaces regular destinations and preserves directory trees" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url

    printf 'replacement' > "$root/mv-overwrite-source"
    printf 'stale' > "$root/mv-overwrite-destination"
    run "$XRDFS" mv \
        "$TEST_ENDPOINT//mv-overwrite-source" \
        "$TEST_ENDPOINT//mv-overwrite-destination"
    assert_success
    run test ! -e "$root/mv-overwrite-source"
    assert_success
    run test "$(cat "$root/mv-overwrite-destination")" = replacement
    assert_success

    mkdir -p \
        "$root/mv-tree-source/nested" \
        "$root/mv-tree-source/empty-child"
    printf 'leaf' > "$root/mv-tree-source/nested/leaf.txt"
    printf 'hidden' > "$root/mv-tree-source/.hidden.txt"
    run "$XRDFS" mv \
        "$TEST_ENDPOINT//mv-tree-source" \
        "$TEST_ENDPOINT//mv-tree-destination"
    assert_success
    run test ! -e "$root/mv-tree-source"
    assert_success
    run test "$(cat "$root/mv-tree-destination/nested/leaf.txt")" = leaf
    assert_success
    run test "$(cat "$root/mv-tree-destination/.hidden.txt")" = hidden
    assert_success
    run test -d "$root/mv-tree-destination/empty-child"
    assert_success
}

@test "mv failures leave the controlled namespace unchanged" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url

    run "$XRDFS" mv \
        "$TEST_ENDPOINT//mv-missing-source" \
        "$TEST_ENDPOINT//mv-missing-destination"
    assert_failure
    run test ! -e "$root/mv-missing-destination"
    assert_success

    printf 'source' > "$root/mv-directory-collision-source"
    mkdir -p "$root/mv-directory-collision-destination"
    printf 'marker' \
        > "$root/mv-directory-collision-destination/marker.txt"
    run "$XRDFS" mv \
        "$TEST_ENDPOINT//mv-directory-collision-source" \
        "$TEST_ENDPOINT//mv-directory-collision-destination"
    assert_failure
    run test "$(cat "$root/mv-directory-collision-source")" = source
    assert_success
    run test \
        "$(cat "$root/mv-directory-collision-destination/marker.txt")" \
        = marker
    assert_success

    printf 'sentinel' > "$root/mv-mixed-endpoint-source"
    run "$XRDFS" mv \
        "$TEST_ENDPOINT//mv-mixed-endpoint-source" \
        root://127.0.0.1:11965//mv-mixed-endpoint-destination
    assert_failure 1
    assert_output 'xrdfs: all URL operands must use the same endpoint'
    run test "$(cat "$root/mv-mixed-endpoint-source")" = sentinel
    assert_success
    run test ! -e "$root/mv-mixed-endpoint-destination"
    assert_success
}

@test "native ROOT removal keeps server-side symlink and rmdir semantics" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url

    run "$XRDFS" rm "$TEST_ENDPOINT//remove-link"
    assert_success
    run test ! -e "$root/remove-link"
    assert_success
    run test -f "$root/data/first.txt"
    assert_success

    run "$XRDFS" rmdir "$TEST_DIRECTORY"
    assert_failure
    run test -f "$root/data/first.txt"
    assert_success
}

@test "recursive rm removes nested, empty, and specially named native trees" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    local tree=$root/remove-recursive
    mkdir -p "$tree/nested/empty" "$tree/standalone-empty" \
        "$tree/special names/-child"
    printf 'top' > "$tree/top.txt"
    printf 'leaf' > "$tree/nested/leaf.txt"
    printf 'hidden' > "$tree/special names/.hidden"
    printf 'space' > "$tree/special names/file with spaces"
    printf 'unicode' > "$tree/special names/unicodé-文件"
    printf 'percent' > "$tree/special names/percent%name"
    printf 'hash' > "$tree/special names/hash#name"

    run "$XRDFS" rm --recursive "$TEST_ENDPOINT//remove-recursive"
    assert_success
    run test ! -e "$tree"
    assert_success
}

@test "recursive rm continues with later operands after a missing target" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    mkdir -p "$root/remove-after-missing-one/nested" \
        "$root/remove-after-missing-two/empty"
    printf 'one' > "$root/remove-after-missing-one/nested/file"
    printf 'two' > "$root/remove-after-missing-two/file"

    run "$XRDFS" rm -R \
        "$TEST_ENDPOINT//missing-recursive-target" \
        "$TEST_ENDPOINT//remove-after-missing-one" \
        "$TEST_ENDPOINT//remove-after-missing-two"
    assert_failure
    run test ! -e "$root/remove-after-missing-one"
    assert_success
    run test ! -e "$root/remove-after-missing-two"
    assert_success
}

@test "recursive rm delimiter handles dash paths without following symlinks" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    mkdir -p "$root/-recursive-dash/nested" \
        "$root/recursive-link-target"
    printf 'inside' > "$root/-recursive-dash/nested/file"
    printf 'preserve' > "$root/recursive-link-target/keep"
    ln -s ../recursive-link-target \
        "$root/-recursive-dash/directory-link"

    run bash -c \
        'printf "cd /\nrm -r -- -recursive-dash\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    run test ! -e "$root/-recursive-dash"
    assert_success
    run test -f "$root/recursive-link-target/keep"
    assert_success
}

@test "recursive rm never follows an absolute directory symlink" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    mkdir -p "$root/absolute-link-target"
    printf 'preserve' > "$root/absolute-link-target/marker"
    ln -s "$root/absolute-link-target" "$root/absolute-directory-link"

    run "$XRDFS" rm -r "$TEST_ENDPOINT//absolute-directory-link"
    assert_failure
    run test -L "$root/absolute-directory-link"
    assert_success
    run test -f "$root/absolute-link-target/marker"
    assert_success
}

@test "recursive rm prevalidates the root guard before any mutation" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    mkdir -p "$root/remove-before-root-guard/nested"
    printf 'keep' > "$root/remove-before-root-guard/nested/file"

    run "$XRDFS" rm -r \
        "$TEST_ENDPOINT//remove-before-root-guard" "$TEST_ENDPOINT//"
    assert_failure
    run test -f "$root/remove-before-root-guard/nested/file"
    assert_success
}

@test "recursive rm handles deep native trees iteratively" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    local tree=$root/remove-deep
    local path=$tree
    mkdir -p "$path"
    for _ in {1..160}; do
        path=$path/d
        mkdir "$path"
    done
    printf 'deep' > "$path/leaf"

    run "$XRDFS" rm -r "$TEST_ENDPOINT//remove-deep"
    assert_success
    run test ! -e "$tree"
    assert_success
}

@test "dry-run inspects native files and recursive trees without changing them" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    mkdir -p "$root/dry-run-tree/nested/empty" \
        "$root/dry-run-tree/standalone-empty"
    printf 'file' > "$root/dry-run-file"
    printf 'top' > "$root/dry-run-tree/top"
    printf 'leaf' > "$root/dry-run-tree/nested/leaf"

    run "$XRDFS" rm --dry-run "$TEST_ENDPOINT//dry-run-file"
    assert_success
    assert_output --partial $'/dry-run-file\tSKIP'
    run test -f "$root/dry-run-file"
    assert_success

    run "$XRDFS" "$TEST_ENDPOINT" rm --dry-run -R /dry-run-tree
    assert_success
    assert_output --partial $'/dry-run-tree/top\tSKIP'
    assert_output --partial $'/dry-run-tree/nested/leaf\tSKIP'
    assert_output --partial $'/dry-run-tree/nested\tSKIP DIR'
    assert_output --partial $'/dry-run-tree\tSKIP DIR'

    local leaf_line=-1 nested_line=-1 root_line=-1 index
    for ((index = 0; index < ${#lines[@]}; ++index)); do
        case "${lines[index]}" in
            $'/dry-run-tree/nested/leaf\tSKIP') leaf_line=$index ;;
            $'/dry-run-tree/nested\tSKIP DIR') nested_line=$index ;;
            $'/dry-run-tree\tSKIP DIR') root_line=$index ;;
        esac
    done
    run test "$leaf_line" -lt "$nested_line"
    assert_success
    run test "$nested_line" -lt "$root_line"
    assert_success
    run test -f "$root/dry-run-tree/top"
    assert_success
    run test -f "$root/dry-run-tree/nested/leaf"
    assert_success
    run test -d "$root/dry-run-tree/nested/empty"
    assert_success
}

@test "recursive dry-run terminates on a directory symlink cycle" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    mkdir -p "$root/dry-run-cycle"
    printf 'preserve' > "$root/dry-run-cycle/marker"
    ln -s . "$root/dry-run-cycle/loop"

    run "$XRDFS" rm -r --dry-run \
        "$TEST_ENDPOINT//dry-run-cycle"
    assert_failure
    assert_output --partial $'/dry-run-cycle/loop'
    run test -f "$root/dry-run-cycle/marker"
    assert_success
    run test -L "$root/dry-run-cycle/loop"
    assert_success
}

@test "dry-run reports a missing root then plans later operands" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    printf 'preserve' > "$root/dry-run-after-missing"

    run "$XRDFS" rm --dry-run \
        "$TEST_ENDPOINT//dry-run-missing" \
        "$TEST_ENDPOINT//dry-run-after-missing"
    assert_failure
    assert_output --partial $'/dry-run-missing\tMISSING'
    assert_output --partial $'/dry-run-after-missing\tSKIP'
    run test -f "$root/dry-run-after-missing"
    assert_success
}

@test "recursive dry-run combines with delimiter for a dash path" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    mkdir -p "$root/-dry-run-tree/nested"
    printf 'preserve' > "$root/-dry-run-tree/nested/file"

    run bash -c \
        'printf "cd /\nrm --dry-run -r -- -dry-run-tree\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    assert_output --partial $'/-dry-run-tree/nested/file\tSKIP'
    assert_output --partial $'/-dry-run-tree\tSKIP DIR'
    run test -f "$root/-dry-run-tree/nested/file"
    assert_success
}

@test "nonrecursive dry-run rejects a directory without changing it" {
    local root=$BATS_TEST_TMPDIR/xrdfs-full-url
    mkdir -p "$root/dry-run-nonrecursive-directory"
    printf 'preserve' > "$root/dry-run-nonrecursive-directory/marker"

    run "$XRDFS" rm --dry-run \
        "$TEST_ENDPOINT//dry-run-nonrecursive-directory"
    assert_failure
    refute_output --partial $'/dry-run-nonrecursive-directory\tSKIP DIR'
    run test -f "$root/dry-run-nonrecursive-directory/marker"
    assert_success
}

@test "WebDAV non-recursive removal verifies targets before DELETE" {
    local mock=$BATS_TEST_TMPDIR/webdav-removal
    local plugins=$mock/client.plugins.d
    local port_file=$mock/port
    local requests=$mock/requests.log
    mkdir -p "$plugins"
    printf '%s\n' \
        'url = http://*;https://*;dav://*;davs://*' \
        'lib = libXrdClHttp.so' \
        'enable = true' > "$plugins/http.conf"

    python3 "$BATS_TEST_DIRNAME/webdav-removal-mock.py" \
        "$port_file" "$requests" &
    local mock_pid=$!
    printf '%s\n' "$mock_pid" > "$mock/webdav-removal.pid"

    local ready=false
    for _ in {1..50}; do
        if [[ -s "$port_file" ]]; then
            ready=true
            break
        fi
        sleep 0.1
    done
    "$ready"

    local endpoint=http://127.0.0.1:$(<"$port_file")

    run env XRD_PLUGINCONFDIR="$plugins" \
        "$XRDFS" rm --dry-run \
        "$endpoint/dry-run-file?authz=file-secret&signature=signed-file"
    assert_success
    assert_output --partial $'/dry-run-file?authz=REDACTED&signature=signed-file\tSKIP'
    refute_output --partial file-secret
    run request_count DELETE /dry-run-file "$requests"
    assert_success
    assert_output 0
    run request_count PROPFIND /dry-run-file "$requests"
    assert_success
    assert_output 1
    run request_query_count PROPFIND /dry-run-file \
        signature=signed-file "$requests"
    assert_success
    assert_output 1

    run env XRD_PLUGINCONFDIR="$plugins" \
        "$XRDFS" rm --dry-run \
        "$endpoint/dry-run-missing?authz=return-secret&signature=signed-missing"
    assert_failure
    assert_output --partial \
        $'/dry-run-missing?authz=REDACTED&signature=signed-missing\tMISSING'
    refute_output --partial return-secret
    run request_count DELETE /dry-run-missing "$requests"
    assert_success
    assert_output 0
    run request_count PROPFIND /dry-run-missing "$requests"
    assert_success
    assert_output 1
    run request_query_count PROPFIND /dry-run-missing \
        signature=signed-missing "$requests"
    assert_success
    assert_output 1

    run env XRD_PLUGINCONFDIR="$plugins" \
        "$XRDFS" rm -r --dry-run \
        "$endpoint/dry-run-tree?authz=tree-secret&signature=signed-tree"
    assert_success
    assert_output --partial \
        $'/dry-run-tree/file?authz=REDACTED&signature=signed-tree\tSKIP'
    assert_output --partial \
        $'/dry-run-tree/nested/leaf?authz=REDACTED&signature=signed-tree\tSKIP'
    assert_output --partial \
        $'/dry-run-tree/nested?authz=REDACTED&signature=signed-tree\tSKIP DIR'
    assert_output --partial \
        $'/dry-run-tree?authz=REDACTED&signature=signed-tree\tSKIP DIR'
    refute_output --partial tree-secret
    run request_count DELETE /dry-run-tree "$requests"
    assert_success
    assert_output 0
    run request_count DELETE /dry-run-tree/file "$requests"
    assert_success
    assert_output 0
    run request_count DELETE /dry-run-tree/nested "$requests"
    assert_success
    assert_output 0
    run request_count DELETE /dry-run-tree/nested/leaf "$requests"
    assert_success
    assert_output 0
    run request_query_count PROPFIND /dry-run-tree signature=signed-tree \
        "$requests"
    assert_success
    assert_output 2
    run request_query_count PROPFIND /dry-run-tree/file signature=signed-tree \
        "$requests"
    assert_success
    assert_output 1
    run request_query_count PROPFIND /dry-run-tree/nested signature=signed-tree \
        "$requests"
    assert_success
    assert_output 2
    run request_query_count PROPFIND /dry-run-tree/nested/leaf \
        signature=signed-tree "$requests"
    assert_success
    assert_output 1

    run env XRD_PLUGINCONFDIR="$plugins" \
        "$XRDFS" rm "$endpoint/file-a" "$endpoint/directory"
    assert_failure
    run request_count DELETE /file-a "$requests"
    assert_success
    assert_output 0
    run request_count DELETE /directory "$requests"
    assert_success
    assert_output 0

    run env XRD_PLUGINCONFDIR="$plugins" "$XRDFS" rm "$endpoint/file"
    assert_success
    run request_count DELETE /file "$requests"
    assert_success
    assert_output 1

    run env XRD_PLUGINCONFDIR="$plugins" \
        "$XRDFS" rmdir "$endpoint/nonempty"
    assert_failure
    run request_count DELETE /nonempty "$requests"
    assert_success
    assert_output 0

    run env XRD_PLUGINCONFDIR="$plugins" "$XRDFS" rmdir "$endpoint/file-a"
    assert_failure
    run request_count DELETE /file-a "$requests"
    assert_success
    assert_output 0

    run env XRD_PLUGINCONFDIR="$plugins" "$XRDFS" rmdir "$endpoint/empty"
    assert_success
    run request_count DELETE /empty "$requests"
    assert_success
    assert_output 1

    # Recursive mode intentionally relies on WebDAV collection DELETE. A 204
    # means the whole collection operation completed, so no client traversal
    # or additional metadata request is needed.
    run env XRD_PLUGINCONFDIR="$plugins" \
        "$XRDFS" rm -r "$endpoint/directory"
    assert_success
    run request_count DELETE /directory "$requests"
    assert_success
    assert_output 1
    run request_count PROPFIND /directory "$requests"
    assert_success
    assert_output 1

    run env XRD_PLUGINCONFDIR="$plugins" \
        "$XRDFS" rm --recursive "$endpoint/partial"
    assert_failure
    run request_count DELETE /partial "$requests"
    assert_success
    assert_output 1
    run request_count PROPFIND /partial "$requests"
    assert_success
    assert_output 0

    run env XRD_PLUGINCONFDIR="$plugins" \
        "$XRDFS" rm --recursive "$endpoint/forbidden"
    assert_failure
    run request_count DELETE /forbidden "$requests"
    assert_success
    assert_output 1
    run request_count PROPFIND /forbidden "$requests"
    assert_success
    assert_output 0

    kill "$mock_pid"
    wait "$mock_pid" 2>/dev/null || true
    : > "$mock/webdav-removal.pid"
}

@test "ls accepts gfal human-readable and directory options" {
    run "$XRDFS" ls -lH "$TEST_DIRECTORY"
    assert_success
    assert_output --partial first.txt
    assert_output --partial '1.1K'

    run "$XRDFS" ls --long --human-readable --directory "$TEST_DIRECTORY"
    assert_success
    assert_output --partial /data/
    refute_output --partial first.txt
}

@test "ls accepts gfal all and uncolored options as no-ops" {
    run "$XRDFS" ls "$TEST_DIRECTORY"
    assert_success
    local native_output=$output
    assert_output --partial .hidden.txt

    run "$XRDFS" ls -a "$TEST_DIRECTORY"
    assert_success
    assert_output "$native_output"

    run "$XRDFS" ls --all --color=never "$TEST_DIRECTORY"
    assert_success
    assert_output "$native_output"

    run "$XRDFS" ls --color never "$TEST_DIRECTORY"
    assert_success
    assert_output "$native_output"

    run "$XRDFS" ls -la "$TEST_DIRECTORY"
    assert_success
    assert_output --partial .hidden.txt
}

@test "ls rejects unsupported gfal presentation options" {
    for option in --time-style=full-iso --full-time --color=auto \
        --unknown-option; do
        run "$XRDFS" ls "$option" "$TEST_DIRECTORY"
        assert_failure
        assert_output --partial 'Invalid arguments'
    done

    run "$XRDFS" ls --color always "$TEST_DIRECTORY"
    assert_failure
    assert_output --partial 'Invalid arguments'
}

@test "ls appends repeatable gfal xattrs only to long output" {
    run "$XRDFS" ls --xattr missing.attribute "$TEST_FILE"
    assert_success
    assert_output /data/first.txt

    run "$XRDFS" ls -l --xattr user.status \
        --xattr=user.checksum.adler32 "$TEST_FILE"
    assert_success
    assert_output --regexp $'\tONLINE\t[[:xdigit:]]{8}$'

    run "$XRDFS" ls -l --xattr missing.attribute "$TEST_FILE"
    assert_failure

    local listing="$BATS_TEST_TMPDIR/ls-xattrs.out"
    run bash -c '"$1" ls -l --xattr user.status \
        --xattr user.checksum.adler32 "$2" >"$3"' \
        _ "$XRDFS" "$TEST_SUBDIRECTORY" "$listing"
    assert_success
    run awk -F '\t' '
        NF != 3 || $2 != "ONLINE" || $3 !~ /^[[:xdigit:]]{8}$/ { exit 1 }
        END { if (NR == 0) exit 1 }
    ' "$listing"
    assert_success
}

@test "ls option delimiter preserves dash-prefixed paths" {
    run bash -c \
        'printf "cd /\nls -- -dash.txt\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    assert_output --partial /-dash.txt
}

@test "legacy and complete-URL cat forms have identical output" {
    run "$XRDFS" "$TEST_ENDPOINT" cat /data/first.txt
    assert_success
    local legacy_output=$output

    run "$XRDFS" cat "$TEST_FILE"
    assert_success
    assert_output "$legacy_output"
    assert_output first
}

@test "cat accepts gfal bytes options and multiple same-endpoint URLs" {
    run "$XRDFS" cat -b "$TEST_FILE" "$TEST_SECOND_FILE"
    assert_success
    assert_output firstsecond

    run "$XRDFS" cat --bytes "$TEST_EMPTY_FILE"
    assert_success
    assert_output ''

    local destination="$BATS_TEST_TMPDIR/binary-download.dat"
    run bash -c '"$1" cat -b "$2" > "$3"' \
        _ "$XRDFS" "$TEST_BINARY_FILE" "$destination"
    assert_success
    run cmp "$BATS_TEST_TMPDIR/xrdfs-full-url/data/binary.dat" "$destination"
    assert_success
}

@test "cat bytes compatibility option still requires a file" {
    run "$XRDFS" "$TEST_ENDPOINT" cat -b
    assert_failure
    assert_output --partial 'Invalid arguments'
}

@test "cat option delimiter preserves dash-prefixed paths" {
    run bash -c \
        'printf "cd /\ncat -- -dash.txt\nexit\n" | "$1" "$2"' \
        _ "$XRDFS" "$TEST_ENDPOINT"
    assert_success
    assert_output --partial dash
}

@test "xattr preserves explicit native list and get forms" {
    run "$XRDFS" "$TEST_ENDPOINT" xattr /data/first.txt list
    assert_success
    local list_output=$output

    run "$XRDFS" xattr "$TEST_FILE" list
    assert_success
    assert_output "$list_output"
    assert_output --partial 'user.test="fixture"'

    run "$XRDFS" "$TEST_ENDPOINT" xattr /data/first.txt get user.test
    assert_success
    local get_output=$output

    run "$XRDFS" xattr "$TEST_FILE" get user.test
    assert_success
    assert_output "$get_output"
}

@test "xattr preserves native set and delete forms" {
    run "$XRDFS" xattr "$TEST_FILE" set user.roundtrip=value=with=equals
    assert_success

    run "$XRDFS" xattr "$TEST_FILE" get user.roundtrip
    assert_success
    assert_output --partial 'user.roundtrip="value=with=equals"'

    run "$XRDFS" xattr "$TEST_FILE" del user.roundtrip
    assert_success

    run "$XRDFS" xattr "$TEST_FILE" get user.roundtrip
    assert_failure
}

@test "xattr shorthand maps gfal virtual attributes to native queries" {
    run "$XRDFS" "$TEST_ENDPOINT" query checksum /data/first.txt
    assert_success
    local checksum=$output

    run "$XRDFS" xattr "$TEST_FILE" xroot.cksum
    assert_success
    assert_output "$checksum"

    run "$XRDFS" xattr "$TEST_FILE" user.checksum.adler32
    assert_success
    assert_output "${checksum#* }"

    run "$XRDFS" xattr "$TEST_FILE" user.status
    assert_success
    assert_output ONLINE

    run "$XRDFS" xattr "$TEST_FILE"
    assert_success
    assert_output --partial "xroot.cksum = $checksum"
    assert_output --partial 'xroot.space = '
    assert_output --partial 'xroot.xattr '
    assert_output --partial 'spacetoken = { "totalsize": '
    assert_output --partial '"unusedsize": '
    assert_output --partial '"usedsize": '
    assert_output --partial '"guaranteedsize": '
}

@test "xattr shorthand falls back to native attributes and handles reserved names" {
    run "$XRDFS" xattr "$TEST_FILE" user.test
    assert_success
    assert_output fixture

    for attribute in list get set del; do
        run "$XRDFS" xattr "$TEST_FILE" set "$attribute=reserved-$attribute"
        assert_success

        run "$XRDFS" xattr "$TEST_FILE" -- "$attribute"
        assert_success
        assert_output "reserved-$attribute"
    done
}

@test "xattr rejects a checksum attribute without an algorithm" {
    run "$XRDFS" xattr "$TEST_FILE" user.checksum.
    assert_failure
    assert_output --partial 'Checksum type cannot be empty'
}

@test "xattr rejects an option delimiter without an attribute" {
    run "$XRDFS" xattr "$TEST_FILE" --
    assert_failure
    assert_output --partial 'Invalid arguments'
}

@test "sum maps gfal positional arguments to the native checksum query" {
    run "$XRDFS" "$TEST_ENDPOINT" query checksum \
        '/data/first.txt?cks.type=adler32'
    assert_success
    local query_output=$output

    run "$XRDFS" sum "$TEST_FILE" ADLER32
    assert_success
    assert_output "$query_output"
    assert_output --regexp '^adler32 [[:xdigit:]]{8}$'

    run "$XRDFS" "$TEST_ENDPOINT" sum /data/first.txt adler32
    assert_success
    assert_output "$query_output"
}

@test "sum preserves existing URL parameters when selecting an algorithm" {
    run "$XRDFS" sum "$TEST_FILE?xrdcl.test=1" ADLER32
    assert_success
    assert_output --regexp '^adler32 [[:xdigit:]]{8}$'
}

@test "sum rejects invalid and unsupported checksum types" {
    run "$XRDFS" sum "$TEST_FILE" 'md5&injected=true'
    assert_failure

    run "$XRDFS" sum "$TEST_FILE" sha256
    assert_failure
}

@test "URL parameters are preserved in the operand path" {
    run "$XRDFS" stat "$TEST_FILE?xrdcl.test=1"
    assert_success
    assert_output --partial 'Size:   5'
}

@test "mixed URL endpoints are rejected before execution" {
    run "$XRDFS" cat "$TEST_FILE" \
        root://127.0.0.1:11965//data/second.txt
    assert_failure 1
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "local and malformed URL operands are rejected" {
    for url in file:///tmp/examplefile FILE://localhost/tmp/examplefile \
        STDIO://-/examplefile '1root://localhost:11965//data/first.txt'; do
        run "$XRDFS" stat "$url"
        assert_failure 1
        assert_output 'xrdfs: invalid remote URL operand'
    done
}

@test "command-first raw queries remain unsupported" {
    run "$XRDFS" query "$TEST_FILE"
    assert_failure 1
    assert_output "xrdfs: command-first full URLs are not supported for 'query'"
}

@test "missing paths fail through the existing command handler" {
    run "$XRDFS" stat "$TEST_ENDPOINT//data/missing.txt"
    refute_output --partial 'invalid remote URL operand'
    assert_failure
}
@test "server-first and command-first syntax cannot be mixed" {
    run -1 "$XRDFS" "$TEST_ENDPOINT" stat "$TEST_FILE"
    assert_output --partial \
        'cannot mix a leading endpoint with full URL operands'
}

@test "URL-like xattr values remain command operands" {
    run -0 "$XRDFS" xattr "$TEST_FILE" set \
        link=https://example.org/resource

    run -0 "$XRDFS" xattr "$TEST_FILE" get link
    assert_output --partial 'link="https://example.org/resource"'
}

@test "multiple URLs on the same endpoint are normalized" {
    local renamed="$TEST_ENDPOINT//data/renamed"
    run -0 "$XRDFS" mv "$TEST_FILE" "$renamed"
    run -0 "$XRDFS" stat "$renamed"
}

@test "mixed URL credentials are rejected before execution" {
    run -1 "$XRDFS" mv \
        root://alice@localhost:11965//data/first.txt \
        root://bob@localhost:11965//data/renamed
    assert_output 'xrdfs: all URL operands must use the same endpoint'
}

@test "invalid remote URLs are rejected" {
    run -1 "$XRDFS" stat root://:11965//data/first.txt
    assert_output 'xrdfs: invalid remote URL operand'
}

@test "server-first query parameters may contain URL-like strings" {
    run "$XRDFS" "$TEST_ENDPOINT" query opaque "$TEST_FILE"
    refute_output --partial 'cannot mix a leading endpoint'
}

@test "debug levels follow xrdcp conventions" {
    run -0 "$XRDFS" -d 2 stat "$TEST_FILE"
    assert_output --partial 'Network Stack:'

    run -1 "$XRDFS" --debug 4 stat "$TEST_FILE"
    assert_output "xrdfs: invalid debug level '4' (expected 0-3)"
}

@test "IPv4 network stack can be selected" {
    run -0 "$XRDFS" -d 2 -4 stat "$TEST_FILE"
    assert_output --partial 'Network Stack: IPv4'
}

@test "IPv6 network stack can be selected" {
    run -0 "$XRDFS" -d 2 -6 stat "$TEST_FILE"
    assert_output --partial 'Network Stack: IPv6'
}

@test "network stack selections are mutually exclusive" {
    run -1 "$XRDFS" --ipv4 --ipv6 stat "$TEST_FILE"
    assert_output 'xrdfs: -4 and -6 are mutually exclusive'
}

@test "ls accepts -1 option matching default single-column output" {
    run "$XRDFS" ls "$TEST_DIRECTORY"
    assert_success
    local default_output=$output

    run "$XRDFS" ls -1 "$TEST_DIRECTORY"
    assert_success
    assert_output "$default_output"

    run "$XRDFS" ls -1a "$TEST_DIRECTORY"
    assert_success
    assert_output "$default_output"
}

@test "rm accepts -f and --force to tolerate missing paths" {
    run "$XRDFS" rm "$TEST_ENDPOINT//data/definitely-missing-file.txt"
    assert_failure

    run "$XRDFS" rm -f "$TEST_ENDPOINT//data/definitely-missing-file.txt"
    assert_success

    run "$XRDFS" rm --force "$TEST_ENDPOINT//data/definitely-missing-file.txt"
    assert_success

    run "$XRDFS" rm -rf "$TEST_ENDPOINT//data/definitely-missing-dir"
    assert_success
}

@test "prepare accepts options with complete URLs" {
    run "$XRDFS" prepare -s --pin-lifetime 3600 "$TEST_FILE"
    assert_success

    run "$XRDFS" prepare -f -w -p 2 "$TEST_FILE"
    assert_success

    run "$XRDFS" prepare -c "$TEST_FILE"
    assert_success
}

@test "query supports server configuration and stats queries" {
    run "$XRDFS" "$TEST_ENDPOINT" query config version
    assert_success

    run "$XRDFS" "$TEST_ENDPOINT" query stats a
    assert_success
}
