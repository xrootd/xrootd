#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

XRDFS=${XRDFS:-xrdfs}
XRDCP=${XRDCP:-xrdcp}

# This suite is a live behavioral comparison with gfal2-util. It is opt-in so
# that gfal2 remains neither a build dependency nor a normal test dependency.
# All remote operations target the ephemeral XRootD server created below.
# Namespace mutations are confined to that localhost fixture, and every copy
# destination is local to BATS_TEST_TMPDIR.

require_gfal2_reference() {
    if [[ "${XRDFS_GFAL2_REFERENCE:-0}" != 1 ]]; then
        skip 'set XRDFS_GFAL2_REFERENCE=1 to run the gfal2 reference suite'
    fi

    local tool
    for tool in gfal-stat gfal-ls gfal-cat gfal-sum gfal-xattr gfal-copy; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            skip "$tool is required by the gfal2 reference suite"
        fi
    done

    # command -v is insufficient for broken Python entry points or incomplete
    # gfal2 installations. Exercise the common CLI bootstrap before launching
    # the server so such environments skip instead of producing false failures.
    if ! gfal-stat --version >/dev/null 2>&1; then
        skip 'the installed gfal2-util commands are not functional'
    fi
}

require_gfal2_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        skip "$1 is required by this gfal2 reference case"
    fi
    if ! "$1" --version >/dev/null 2>&1; then
        skip "the installed $1 command is not functional"
    fi
}

require_live_reference_files() {
    if [[ -z "${XRDFS_GFAL2_LIVE_ROOT_FILE:-}" ||
          -z "${XRDFS_GFAL2_LIVE_HTTPS_FILE:-}" ]]; then
        skip 'set XRDFS_GFAL2_LIVE_ROOT_FILE and XRDFS_GFAL2_LIVE_HTTPS_FILE'
    fi
}

require_live_reference_directories() {
    if [[ -z "${XRDFS_GFAL2_LIVE_ROOT_DIR:-}" ||
          -z "${XRDFS_GFAL2_LIVE_HTTPS_DIR:-}" ]]; then
        skip 'set XRDFS_GFAL2_LIVE_ROOT_DIR and XRDFS_GFAL2_LIVE_HTTPS_DIR'
    fi
}

write_all_bytes() {
    local value octal
    for ((value = 0; value < 256; ++value)); do
        printf -v octal '%03o' "$value"
        printf '%b' "\\$octal"
    done
}

record() {
    local stdout_file=$1
    local stderr_file=$2
    shift 2
    "$@" >"$stdout_file" 2>"$stderr_file"
}

record_combined() {
    local output_file=$1
    shift
    "$@" >"$output_file" 2>&1
}

stat_size() {
    sed -n \
        's/^[[:space:]]*Size:[[:space:]]*\([0-9][0-9]*\).*/\1/p' \
        "$1" | head -n 1
}

normalize_listing() {
    sed -e 's|/*$||' -e 's|.*/||' "$1" | LC_ALL=C sort
}

checksum_digest() {
    awk 'NF { print $NF; exit }' "$1" | tr '[:upper:]' '[:lower:]'
}

local_mode() {
    if stat -c '%a' "$1" >/dev/null 2>&1; then
        stat -c '%a' "$1"
    else
        stat -f '%Lp' "$1"
    fi
}

producer_pipe_status() {
    set +e
    "$@" 2>/dev/null | head -c 1 >/dev/null
    local producer_status=${PIPESTATUS[0]}
    set -e
    printf '%s\n' "$producer_status"
}

setup() {
    require_gfal2_reference
    umask 022

    local root="$BATS_TEST_TMPDIR/xrdfs-gfal2-reference"
    mkdir -p \
        "$root/data/subdir" \
        "$root/data/empty-dir" \
        "$root/data/tree/nested" \
        "$root/data/tree/empty-child"

    printf 'first' >"$root/data/regular.txt"
    printf 'second' >"$root/data/second.txt"
    : >"$root/data/empty.txt"
    printf 'hidden' >"$root/data/.hidden.txt"
    printf 'dash' >"$root/data/-leading.txt"
    printf 'space' >"$root/data/space name.txt"
    printf 'nested' >"$root/data/subdir/nested.txt"
    printf 'leaf' >"$root/data/tree/nested/leaf.txt"
    printf 'tree-hidden' >"$root/data/tree/.tree-hidden"
    write_all_bytes >"$root/data/all-bytes.dat"
    head -c 1025 /dev/zero >"$root/data/size-1025.dat"
    head -c 8388608 /dev/zero >"$root/data/large.dat"
    ln -s regular.txt "$root/data/good-link"
    ln -s missing-target.txt "$root/data/dangling-link"

    launch_xrootd xrdfs-gfal2-reference.cfg xrdfs-gfal2-reference

    export TEST_ENDPOINT=root://localhost:11966
    export TEST_DIRECTORY=$TEST_ENDPOINT//data/
    export TEST_CLEAN_DIRECTORY=$TEST_ENDPOINT//data/subdir/
    export TEST_REGULAR=$TEST_ENDPOINT//data/regular.txt
    export TEST_SECOND=$TEST_ENDPOINT//data/second.txt
    export TEST_EMPTY=$TEST_ENDPOINT//data/empty.txt
    export TEST_ALL_BYTES=$TEST_ENDPOINT//data/all-bytes.dat
    export TEST_LARGE=$TEST_ENDPOINT//data/large.dat
    export TEST_SIZE_1025=$TEST_ENDPOINT//data/size-1025.dat
    export TEST_GOOD_LINK=$TEST_ENDPOINT//data/good-link
    export TEST_DANGLING_LINK=$TEST_ENDPOINT//data/dangling-link
    export TEST_MISSING=$TEST_ENDPOINT//data/missing.txt
    export TEST_EMPTY_DIRECTORY=$TEST_ENDPOINT//data/empty-dir/
    export TEST_TREE=$TEST_ENDPOINT//data/tree/
    export TEST_SPACE_NAME=$TEST_ENDPOINT//data/space\ name.txt

    local ready=false
    for _ in {1..50}; do
        if "$XRDFS" "$TEST_ENDPOINT" stat /data/regular.txt \
            >/dev/null 2>&1; then
            ready=true
            break
        fi
        sleep 0.1
    done
    "$ready"

    # Seed a native XRootD attribute on the controlled fixture. Tests access it
    # only through read-only list/get operations.
    "$XRDFS" "$TEST_ENDPOINT" xattr /data/regular.txt set \
        user.fixture=fixture >/dev/null
}

teardown() {
    kill_pid_files 2>/dev/null || true
}

bats::on_failure() {
    print_log_files
}

@test "reference mkdir: explicit octal modes and multiple operands agree" {
    require_gfal2_command gfal-mkdir
    local root=$BATS_TEST_TMPDIR/xrdfs-gfal2-reference
    local gfal_one=$TEST_ENDPOINT//mkdir-reference/gfal-one
    local gfal_two=$TEST_ENDPOINT//mkdir-reference/gfal-two
    local xrd_one=$TEST_ENDPOINT//mkdir-reference/xrd-one
    local xrd_two=$TEST_ENDPOINT//mkdir-reference/xrd-two

    run gfal-mkdir -p -m 0751 "$gfal_one" "$gfal_two"
    assert_success
    run "$XRDFS" mkdir --parents --mode=0751 "$xrd_one" "$xrd_two"
    assert_success

    run local_mode "$root/mkdir-reference/gfal-one"
    assert_success
    local gfal_mode=$output
    run local_mode "$root/mkdir-reference/xrd-one"
    assert_success
    assert_output "$gfal_mode"

    run local_mode "$root/mkdir-reference/gfal-two"
    assert_success
    gfal_mode=$output
    run local_mode "$root/mkdir-reference/xrd-two"
    assert_success
    assert_output "$gfal_mode"
}

@test "reference mkdir: the documented default mode remains distinct" {
    require_gfal2_command gfal-mkdir
    local root=$BATS_TEST_TMPDIR/xrdfs-gfal2-reference

    run gfal-mkdir "$TEST_ENDPOINT//gfal-default-mode"
    assert_success
    run "$XRDFS" mkdir "$TEST_ENDPOINT//xrdfs-default-mode"
    assert_success

    run local_mode "$root/gfal-default-mode"
    assert_success
    assert_output 755
    run local_mode "$root/xrdfs-default-mode"
    assert_success
    assert_output 750
}

@test "reference chmod: gfal and xrdfs mode-first forms agree" {
    require_gfal2_command gfal-chmod
    local root=$BATS_TEST_TMPDIR/xrdfs-gfal2-reference
    local gfal_path=$TEST_ENDPOINT//gfal-chmod-reference
    local xrd_path=$TEST_ENDPOINT//xrdfs-chmod-reference

    run "$XRDFS" mkdir "$gfal_path" "$xrd_path"
    assert_success
    run gfal-chmod 0715 "$gfal_path"
    assert_success
    run "$XRDFS" chmod 0715 "$xrd_path"
    assert_success

    run local_mode "$root/gfal-chmod-reference"
    assert_success
    local gfal_mode=$output
    run local_mode "$root/xrdfs-chmod-reference"
    assert_success
    assert_output "$gfal_mode"
}

@test "reference rename: regular-file replacement agrees" {
    require_gfal2_command gfal-rename
    local root=$BATS_TEST_TMPDIR/xrdfs-gfal2-reference

    printf 'replacement' > "$root/gfal-rename-source"
    printf 'replacement' > "$root/xrdfs-mv-source"
    printf 'stale' > "$root/gfal-rename-destination"
    printf 'stale' > "$root/xrdfs-mv-destination"

    run gfal-rename \
        "$TEST_ENDPOINT//gfal-rename-source" \
        "$TEST_ENDPOINT//gfal-rename-destination"
    assert_success
    run "$XRDFS" mv \
        "$TEST_ENDPOINT//xrdfs-mv-source" \
        "$TEST_ENDPOINT//xrdfs-mv-destination"
    assert_success

    run test ! -e "$root/gfal-rename-source"
    assert_success
    run test ! -e "$root/xrdfs-mv-source"
    assert_success
    run cmp \
        "$root/gfal-rename-destination" \
        "$root/xrdfs-mv-destination"
    assert_success
    run test "$(cat "$root/xrdfs-mv-destination")" = replacement
    assert_success
}

@test "reference rename: directory trees agree" {
    require_gfal2_command gfal-rename
    local root=$BATS_TEST_TMPDIR/xrdfs-gfal2-reference

    mkdir -p \
        "$root/gfal-rename-tree/nested" \
        "$root/gfal-rename-tree/empty-child" \
        "$root/xrdfs-mv-tree/nested" \
        "$root/xrdfs-mv-tree/empty-child"
    printf 'leaf' > "$root/gfal-rename-tree/nested/leaf.txt"
    printf 'leaf' > "$root/xrdfs-mv-tree/nested/leaf.txt"
    printf 'hidden' > "$root/gfal-rename-tree/.hidden.txt"
    printf 'hidden' > "$root/xrdfs-mv-tree/.hidden.txt"

    run gfal-rename \
        "$TEST_ENDPOINT//gfal-rename-tree" \
        "$TEST_ENDPOINT//gfal-renamed-tree"
    assert_success
    run "$XRDFS" mv \
        "$TEST_ENDPOINT//xrdfs-mv-tree" \
        "$TEST_ENDPOINT//xrdfs-moved-tree"
    assert_success

    run test ! -e "$root/gfal-rename-tree"
    assert_success
    run test ! -e "$root/xrdfs-mv-tree"
    assert_success
    run diff -r "$root/gfal-renamed-tree" "$root/xrdfs-moved-tree"
    assert_success
}

@test "reference rename: missing and directory destinations fail safely" {
    require_gfal2_command gfal-rename
    local root=$BATS_TEST_TMPDIR/xrdfs-gfal2-reference

    run gfal-rename \
        "$TEST_ENDPOINT//gfal-missing-rename-source" \
        "$TEST_ENDPOINT//gfal-missing-rename-destination"
    assert_failure
    run "$XRDFS" mv \
        "$TEST_ENDPOINT//xrdfs-missing-mv-source" \
        "$TEST_ENDPOINT//xrdfs-missing-mv-destination"
    assert_failure
    run test ! -e "$root/gfal-missing-rename-destination"
    assert_success
    run test ! -e "$root/xrdfs-missing-mv-destination"
    assert_success

    printf 'source' > "$root/gfal-directory-collision-source"
    printf 'source' > "$root/xrdfs-directory-collision-source"
    mkdir -p \
        "$root/gfal-directory-collision-destination" \
        "$root/xrdfs-directory-collision-destination"
    printf 'marker' \
        > "$root/gfal-directory-collision-destination/marker.txt"
    printf 'marker' \
        > "$root/xrdfs-directory-collision-destination/marker.txt"

    run gfal-rename \
        "$TEST_ENDPOINT//gfal-directory-collision-source" \
        "$TEST_ENDPOINT//gfal-directory-collision-destination"
    assert_failure
    run "$XRDFS" mv \
        "$TEST_ENDPOINT//xrdfs-directory-collision-source" \
        "$TEST_ENDPOINT//xrdfs-directory-collision-destination"
    assert_failure

    run test "$(cat "$root/gfal-directory-collision-source")" = source
    assert_success
    run test "$(cat "$root/xrdfs-directory-collision-source")" = source
    assert_success
    run test \
        "$(cat "$root/gfal-directory-collision-destination/marker.txt")" \
        = marker
    assert_success
    run test \
        "$(cat "$root/xrdfs-directory-collision-destination/marker.txt")" \
        = marker
    assert_success
}

@test "reference rm: recursive nested and empty trees agree" {
    require_gfal2_command gfal-rm
    local root=$BATS_TEST_TMPDIR/xrdfs-gfal2-reference
    local gfal_tree=$root/rm-reference/gfal-tree
    local xrdfs_tree=$root/rm-reference/xrdfs-tree
    mkdir -p "$gfal_tree/nested/empty" "$gfal_tree/empty-child" \
        "$xrdfs_tree/nested/empty" "$xrdfs_tree/empty-child"
    printf 'gfal' > "$gfal_tree/nested/file"
    printf 'xrdfs' > "$xrdfs_tree/nested/file"

    run gfal-rm -r "$TEST_ENDPOINT//rm-reference/gfal-tree"
    assert_success
    run "$XRDFS" rm --recursive \
        "$TEST_ENDPOINT//rm-reference/xrdfs-tree"
    assert_success

    run test ! -e "$gfal_tree"
    assert_success
    run test ! -e "$xrdfs_tree"
    assert_success
}

@test "reference rm: recursive dry-run plans agree without mutation" {
    require_gfal2_command gfal-rm
    local root=$BATS_TEST_TMPDIR/xrdfs-gfal2-reference
    local gfal_tree=$root/rm-reference/gfal-dry-run-tree
    local xrdfs_tree=$root/rm-reference/xrdfs-dry-run-tree
    mkdir -p "$gfal_tree/nested/empty" "$xrdfs_tree/nested/empty"
    printf 'gfal' > "$gfal_tree/nested/file"
    printf 'xrdfs' > "$xrdfs_tree/nested/file"

    run gfal-rm -r --dry-run \
        "$TEST_ENDPOINT//rm-reference/gfal-dry-run-tree"
    assert_success
    assert_output --partial $'/nested/file\tSKIP'
    assert_output --partial $'/rm-reference/gfal-dry-run-tree\tSKIP DIR'

    run "$XRDFS" rm --dry-run --recursive \
        "$TEST_ENDPOINT//rm-reference/xrdfs-dry-run-tree"
    assert_success
    assert_output --partial $'/nested/file\tSKIP'
    assert_output --partial $'/rm-reference/xrdfs-dry-run-tree\tSKIP DIR'

    run test -f "$gfal_tree/nested/file"
    assert_success
    run test -d "$gfal_tree/nested/empty"
    assert_success
    run test -f "$xrdfs_tree/nested/file"
    assert_success
    run test -d "$xrdfs_tree/nested/empty"
    assert_success
}

@test "reference rm: dry-run missing and later file outcomes agree" {
    require_gfal2_command gfal-rm
    local root=$BATS_TEST_TMPDIR/xrdfs-gfal2-reference
    printf 'gfal' > "$root/rm-reference/gfal-dry-run-later"
    printf 'xrdfs' > "$root/rm-reference/xrdfs-dry-run-later"

    run gfal-rm --dry-run \
        "$TEST_ENDPOINT//rm-reference/gfal-dry-run-missing" \
        "$TEST_ENDPOINT//rm-reference/gfal-dry-run-later"
    assert_failure
    assert_output --partial $'/gfal-dry-run-missing\tMISSING'
    assert_output --partial $'/gfal-dry-run-later\tSKIP'

    run "$XRDFS" rm --dry-run \
        "$TEST_ENDPOINT//rm-reference/xrdfs-dry-run-missing" \
        "$TEST_ENDPOINT//rm-reference/xrdfs-dry-run-later"
    assert_failure
    assert_output --partial $'/xrdfs-dry-run-missing\tMISSING'
    assert_output --partial $'/xrdfs-dry-run-later\tSKIP'

    run test -f "$root/rm-reference/gfal-dry-run-later"
    assert_success
    run test -f "$root/rm-reference/xrdfs-dry-run-later"
    assert_success
}

@test "reference rm: nonrecursive directory rejection agrees" {
    require_gfal2_command gfal-rm
    local root=$BATS_TEST_TMPDIR/xrdfs-gfal2-reference
    local gfal_tree=$root/rm-reference/gfal-nonrecursive
    local xrdfs_tree=$root/rm-reference/xrdfs-nonrecursive
    mkdir -p "$gfal_tree" "$xrdfs_tree"
    printf 'keep' > "$gfal_tree/marker"
    printf 'keep' > "$xrdfs_tree/marker"

    run gfal-rm "$TEST_ENDPOINT//rm-reference/gfal-nonrecursive"
    assert_failure
    run "$XRDFS" rm "$TEST_ENDPOINT//rm-reference/xrdfs-nonrecursive"
    assert_failure

    run test -f "$gfal_tree/marker"
    assert_success
    run test -f "$xrdfs_tree/marker"
    assert_success
}

@test "reference stat: regular and empty files have matching size semantics" {
    local name expected gfal_out xrdfs_out
    for name in regular empty; do
        case "$name" in
            regular) expected=5; url=$TEST_REGULAR ;;
            empty) expected=0; url=$TEST_EMPTY ;;
        esac
        gfal_out="$BATS_TEST_TMPDIR/stat-$name-gfal.out"
        xrdfs_out="$BATS_TEST_TMPDIR/stat-$name-xrdfs.out"

        run record "$gfal_out" "$gfal_out.err" gfal-stat "$url"
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" stat "$url"
        assert_success

        run test "$(stat_size "$gfal_out")" = "$expected"
        assert_success
        run test "$(stat_size "$xrdfs_out")" = "$expected"
        assert_success
    done
}

@test "reference stat: directories and followed symlinks agree semantically" {
    local gfal_out="$BATS_TEST_TMPDIR/stat-gfal.out"
    local xrdfs_out="$BATS_TEST_TMPDIR/stat-xrdfs.out"

    run record "$gfal_out" "$gfal_out.err" gfal-stat "$TEST_DIRECTORY"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" stat "$TEST_DIRECTORY"
    assert_success
    run grep -Eiq 'directory' "$gfal_out"
    assert_success
    run grep -F 'IsDir' "$xrdfs_out"
    assert_success

    run record "$gfal_out" "$gfal_out.err" gfal-stat "$TEST_GOOD_LINK"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" stat "$TEST_GOOD_LINK"
    assert_success
    run test "$(stat_size "$gfal_out")" = 5
    assert_success
    run test "$(stat_size "$xrdfs_out")" = 5
    assert_success
}

@test "reference stat: dangling symlinks and missing paths fail in both tools" {
    local url
    for url in "$TEST_DANGLING_LINK" "$TEST_MISSING"; do
        run gfal-stat "$url"
        assert_failure
        run "$XRDFS" stat "$url"
        assert_failure
    done
}

@test "reference stat and cat: quoted spaces and leading dashes remain data" {
    local url expected
    for url in "$TEST_SPACE_NAME" "$TEST_ENDPOINT//data/-leading.txt"; do
        case "$url" in
            *space*) expected=space ;;
            *) expected=dash ;;
        esac

        run gfal-stat "$url"
        assert_success
        run "$XRDFS" stat "$url"
        assert_success
        run gfal-cat -b "$url"
        assert_success
        assert_output "$expected"
        run "$XRDFS" cat -b "$url"
        assert_success
        assert_output "$expected"
    done
}

@test "reference ls: visible entry sets match except hidden and dangling entries" {
    local gfal_out="$BATS_TEST_TMPDIR/ls-gfal.out"
    local xrdfs_out="$BATS_TEST_TMPDIR/ls-xrdfs.out"
    local gfal_names="$BATS_TEST_TMPDIR/ls-gfal.names"
    local xrdfs_names="$BATS_TEST_TMPDIR/ls-xrdfs.names"

    run record "$gfal_out" "$gfal_out.err" gfal-ls "$TEST_DIRECTORY"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" ls "$TEST_DIRECTORY"
    assert_success
    normalize_listing "$gfal_out" >"$gfal_names"
    normalize_listing "$xrdfs_out" >"$xrdfs_names"

    run grep -Fx .hidden.txt "$gfal_names"
    assert_failure
    run grep -Fx .hidden.txt "$xrdfs_names"
    assert_success
    run grep -Fx dangling-link "$gfal_names"
    assert_failure
    run grep -Fx dangling-link "$xrdfs_names"
    assert_success

    grep -Ev '^(\.hidden\.txt|dangling-link)$' \
        "$xrdfs_names" >"$xrdfs_names.visible"
    run cmp "$gfal_names" "$xrdfs_names.visible"
    assert_success
}

@test "reference ls: -a, --all, and --color=never are xrdfs compatibility no-ops" {
    local default_out="$BATS_TEST_TMPDIR/ls-default.out"
    local short_out="$BATS_TEST_TMPDIR/ls-short.out"
    local long_out="$BATS_TEST_TMPDIR/ls-long.out"
    local color_out="$BATS_TEST_TMPDIR/ls-color.out"
    local gfal_out="$BATS_TEST_TMPDIR/ls-gfal-all.out"

    run record "$default_out" "$default_out.err" \
        "$XRDFS" ls "$TEST_DIRECTORY"
    assert_success
    run record "$short_out" "$short_out.err" \
        "$XRDFS" ls -a "$TEST_DIRECTORY"
    assert_success
    run record "$long_out" "$long_out.err" \
        "$XRDFS" ls --all "$TEST_DIRECTORY"
    assert_success
    run record "$color_out" "$color_out.err" \
        "$XRDFS" ls --color=never "$TEST_DIRECTORY"
    assert_success
    run cmp "$default_out" "$short_out"
    assert_success
    run cmp "$default_out" "$long_out"
    assert_success
    run cmp "$default_out" "$color_out"
    assert_success

    run record "$gfal_out" "$gfal_out.err" gfal-ls -a "$TEST_DIRECTORY"
    assert_success
    normalize_listing "$gfal_out" >"$gfal_out.names"
    run grep -Fx .hidden.txt "$gfal_out.names"
    assert_success
}

@test "reference ls: empty, human-readable, directory, and symlink cases agree" {
    local gfal_out="$BATS_TEST_TMPDIR/ls-gfal.out"
    local xrdfs_out="$BATS_TEST_TMPDIR/ls-xrdfs.out"

    run record "$gfal_out" "$gfal_out.err" gfal-ls "$TEST_EMPTY_DIRECTORY"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" \
        "$XRDFS" ls "$TEST_EMPTY_DIRECTORY"
    assert_success
    run test ! -s "$gfal_out"
    assert_success
    run test ! -s "$xrdfs_out"
    assert_success

    run record "$gfal_out" "$gfal_out.err" gfal-ls -lH "$TEST_SIZE_1025"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" \
        "$XRDFS" ls -lH "$TEST_SIZE_1025"
    assert_success
    run grep -F '1.1K' "$gfal_out"
    assert_success
    run grep -F '1.1K' "$xrdfs_out"
    assert_success

    run record "$gfal_out" "$gfal_out.err" gfal-ls -d "$TEST_DIRECTORY"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" \
        "$XRDFS" ls -d "$TEST_DIRECTORY"
    assert_success
    run test "$(wc -l <"$gfal_out" | tr -d ' ')" = 1
    assert_success
    run test "$(wc -l <"$xrdfs_out" | tr -d ' ')" = 1
    assert_success

    run gfal-ls -d "$TEST_GOOD_LINK"
    assert_success
    run "$XRDFS" ls -d "$TEST_GOOD_LINK"
    assert_success
}

@test "reference ls: repeatable long-list xattrs match and short output ignores them" {
    local gfal_out="$BATS_TEST_TMPDIR/ls-xattr-gfal.out"
    local xrdfs_out="$BATS_TEST_TMPDIR/ls-xattr-xrdfs.out"

    run record "$gfal_out" "$gfal_out.err" gfal-ls -l \
        --xattr user.status --xattr user.checksum.adler32 "$TEST_REGULAR"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" ls -l \
        --xattr user.status --xattr user.checksum.adler32 "$TEST_REGULAR"
    assert_success
    run test "$(cut -f2- "$gfal_out")" = "$(cut -f2- "$xrdfs_out")"
    assert_success
    run test "$(cut -f2 "$xrdfs_out")" = ONLINE
    assert_success
    run test "$(cut -f3 "$xrdfs_out")" = \
        "$(checksum_digest "$xrdfs_out")"
    assert_success

    run gfal-ls --xattr missing.attribute "$TEST_REGULAR"
    assert_success
    run "$XRDFS" ls --xattr missing.attribute "$TEST_REGULAR"
    assert_success

    run record "$gfal_out" "$gfal_out.err" gfal-ls -l \
        --xattr missing.attribute "$TEST_REGULAR"
    assert_failure
    run test ! -s "$gfal_out"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" ls -l \
        --xattr missing.attribute "$TEST_REGULAR"
    assert_failure
    run test ! -s "$xrdfs_out"
    assert_success

    run record "$gfal_out" "$gfal_out.err" gfal-ls -l \
        --xattr user.status --xattr user.checksum.adler32 \
        "$TEST_CLEAN_DIRECTORY"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" ls -l \
        --xattr user.status --xattr user.checksum.adler32 \
        "$TEST_CLEAN_DIRECTORY"
    assert_success
    run test "$(cut -f2- "$gfal_out")" = "$(cut -f2- "$xrdfs_out")"
    assert_success
    run test "$(wc -l <"$gfal_out" | tr -d ' ')" = 1
    assert_success

    run record "$gfal_out" "$gfal_out.err" gfal-ls -l \
        --xattr user.status "$TEST_DIRECTORY"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" ls -l \
        --xattr user.status "$TEST_DIRECTORY"
    assert_success
    run grep -F dangling-link "$gfal_out"
    assert_failure
    run grep -F dangling-link "$xrdfs_out"
    assert_failure
    run awk -F '\t' 'NF != 2 || $2 != "ONLINE" { exit 1 }' "$xrdfs_out"
    assert_success
}

@test "reference cat: byte mode is exact for empty, binary, large, and symlink files" {
    local name url local_file gfal_out xrdfs_out
    for name in regular empty all-bytes large good-link; do
        case "$name" in
            regular) url=$TEST_REGULAR; local_file=regular.txt ;;
            empty) url=$TEST_EMPTY; local_file=empty.txt ;;
            all-bytes) url=$TEST_ALL_BYTES; local_file=all-bytes.dat ;;
            large) url=$TEST_LARGE; local_file=large.dat ;;
            good-link) url=$TEST_GOOD_LINK; local_file=good-link ;;
        esac
        gfal_out="$BATS_TEST_TMPDIR/cat-$name-gfal.out"
        xrdfs_out="$BATS_TEST_TMPDIR/cat-$name-xrdfs.out"

        run record "$gfal_out" "$gfal_out.err" gfal-cat -b "$url"
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" cat -b "$url"
        assert_success
        run cmp "$gfal_out" "$xrdfs_out"
        assert_success
        run cmp "$gfal_out" \
            "$BATS_TEST_TMPDIR/xrdfs-gfal2-reference/data/$local_file"
        assert_success
    done
}

@test "reference cat: xrdfs remains byte-safe where gfal text mode rejects invalid UTF-8" {
    local gfal_out="$BATS_TEST_TMPDIR/cat-text-gfal.out"
    local xrdfs_out="$BATS_TEST_TMPDIR/cat-text-xrdfs.out"

    run record "$gfal_out" "$gfal_out.err" gfal-cat "$TEST_ALL_BYTES"
    assert_failure
    run record "$xrdfs_out" "$xrdfs_out.err" \
        "$XRDFS" cat "$TEST_ALL_BYTES"
    assert_success
    run cmp "$xrdfs_out" \
        "$BATS_TEST_TMPDIR/xrdfs-gfal2-reference/data/all-bytes.dat"
    assert_success
}

@test "reference cat: successful multi-file concatenation is byte-identical" {
    local expected="$BATS_TEST_TMPDIR/cat-expected.out"
    local gfal_out="$BATS_TEST_TMPDIR/cat-gfal.out"
    local xrdfs_out="$BATS_TEST_TMPDIR/cat-xrdfs.out"
    printf 'firstsecond' >"$expected"

    run record "$gfal_out" "$gfal_out.err" \
        gfal-cat -b "$TEST_REGULAR" "$TEST_EMPTY" "$TEST_SECOND"
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" \
        "$XRDFS" cat -b "$TEST_REGULAR" "$TEST_EMPTY" "$TEST_SECOND"
    assert_success
    run cmp "$expected" "$gfal_out"
    assert_success
    run cmp "$expected" "$xrdfs_out"
    assert_success
}

@test "reference cat: gfal stops at a missing operand while xrdfs finishes readable jobs" {
    local gfal_out="$BATS_TEST_TMPDIR/cat-gfal.out"
    local xrdfs_out="$BATS_TEST_TMPDIR/cat-xrdfs.out"
    local expected="$BATS_TEST_TMPDIR/cat-expected.out"

    # Missing first: gfal emits nothing; xrdfs still emits the later file.
    run record "$gfal_out" "$gfal_out.err" \
        gfal-cat -b "$TEST_MISSING" "$TEST_SECOND"
    assert_failure
    run record "$xrdfs_out" "$xrdfs_out.err" \
        "$XRDFS" cat -b "$TEST_MISSING" "$TEST_SECOND"
    assert_failure
    run test ! -s "$gfal_out"
    assert_success
    printf second >"$expected"
    run cmp "$expected" "$xrdfs_out"
    assert_success

    # Missing middle: gfal preserves only the prefix; xrdfs emits both readable
    # operands in their original order.
    run record "$gfal_out" "$gfal_out.err" \
        gfal-cat -b "$TEST_REGULAR" "$TEST_MISSING" "$TEST_SECOND"
    assert_failure
    run record "$xrdfs_out" "$xrdfs_out.err" \
        "$XRDFS" cat -b "$TEST_REGULAR" "$TEST_MISSING" "$TEST_SECOND"
    assert_failure
    printf first >"$expected"
    run cmp "$expected" "$gfal_out"
    assert_success
    printf firstsecond >"$expected"
    run cmp "$expected" "$xrdfs_out"
    assert_success

    # Missing last: both have already emitted the same readable prefix.
    run record "$gfal_out" "$gfal_out.err" \
        gfal-cat -b "$TEST_REGULAR" "$TEST_SECOND" "$TEST_MISSING"
    assert_failure
    run record "$xrdfs_out" "$xrdfs_out.err" \
        "$XRDFS" cat -b "$TEST_REGULAR" "$TEST_SECOND" "$TEST_MISSING"
    assert_failure
    run cmp "$gfal_out" "$xrdfs_out"
    assert_success
}

@test "reference cat: broken pipes use the tools' established distinct exit codes" {
    run producer_pipe_status gfal-cat -b "$TEST_LARGE"
    assert_output 255

    run producer_pipe_status "$XRDFS" cat -b "$TEST_LARGE"
    assert_output 141
}

@test "reference sum: ADLER32 digests match for regular and empty files" {
    local name url gfal_out xrdfs_out
    for name in regular empty; do
        case "$name" in
            regular) url=$TEST_REGULAR ;;
            empty) url=$TEST_EMPTY ;;
        esac
        gfal_out="$BATS_TEST_TMPDIR/sum-$name-gfal.out"
        xrdfs_out="$BATS_TEST_TMPDIR/sum-$name-xrdfs.out"

        run record "$gfal_out" "$gfal_out.err" gfal-sum "$url" ADLER32
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" \
            "$XRDFS" sum "$url" ADLER32
        assert_success
        run test "$(checksum_digest "$gfal_out")" = \
            "$(checksum_digest "$xrdfs_out")"
        assert_success
    done
}

@test "reference sum: missing paths and directories fail in both tools" {
    local url
    for url in "$TEST_MISSING" "$TEST_DIRECTORY"; do
        run gfal-sum "$url" ADLER32
        assert_failure
        run "$XRDFS" sum "$url" ADLER32
        assert_failure
    done
}

@test "reference xattr: virtual checksum and status values match exactly" {
    local attribute gfal_out xrdfs_out
    for attribute in xroot.cksum user.checksum.adler32 user.status; do
        gfal_out="$BATS_TEST_TMPDIR/xattr-$attribute-gfal.out"
        xrdfs_out="$BATS_TEST_TMPDIR/xattr-$attribute-xrdfs.out"

        run record "$gfal_out" "$gfal_out.err" \
            gfal-xattr "$TEST_REGULAR" "$attribute"
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" \
            "$XRDFS" xattr "$TEST_REGULAR" "$attribute"
        assert_success
        run test "$(tr '[:upper:]' '[:lower:]' <"$gfal_out")" = \
            "$(tr '[:upper:]' '[:lower:]' <"$xrdfs_out")"
        assert_success
    done
}

@test "reference xattr: space and opaque metadata attributes share schemas" {
    local attribute gfal_out xrdfs_out field
    for attribute in xroot.space spacetoken; do
        gfal_out="$BATS_TEST_TMPDIR/xattr-$attribute-gfal.out"
        xrdfs_out="$BATS_TEST_TMPDIR/xattr-$attribute-xrdfs.out"

        run record "$gfal_out" "$gfal_out.err" \
            gfal-xattr "$TEST_REGULAR" "$attribute"
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" \
            "$XRDFS" xattr "$TEST_REGULAR" "$attribute"
        assert_success

        if [[ "$attribute" == xroot.space ]]; then
            for field in oss.cgroup oss.space oss.free oss.maxf oss.used \
                oss.quota; do
                run grep -F "$field=" "$gfal_out"
                assert_success
                run grep -F "$field=" "$xrdfs_out"
                assert_success
            done
        else
            for field in totalsize unusedsize usedsize guaranteedsize; do
                run grep -F "\"$field\"" "$gfal_out"
                assert_success
                run grep -F "\"$field\"" "$xrdfs_out"
                assert_success
            done
        fi
    done

    gfal_out="$BATS_TEST_TMPDIR/xattr-xroot.xattr-gfal.out"
    xrdfs_out="$BATS_TEST_TMPDIR/xattr-xroot.xattr-xrdfs.out"
    run record "$gfal_out" "$gfal_out.err" \
        gfal-xattr "$TEST_REGULAR" xroot.xattr
    assert_success
    run record "$xrdfs_out" "$xrdfs_out.err" \
        "$XRDFS" xattr "$TEST_REGULAR" xroot.xattr
    assert_success
    for field in oss.cgroup oss.type oss.used oss.mt oss.ct oss.at oss.u \
        oss.g oss.fs ofs.ap; do
        run grep -F "$field=" "$gfal_out"
        assert_success
        run grep -F "$field=" "$xrdfs_out"
        assert_success
    done
}

@test "reference xattr: virtual lists are distinct from native XRootD fattrs" {
    local gfal_virtual="$BATS_TEST_TMPDIR/xattr-gfal-virtual.out"
    local xrdfs_virtual="$BATS_TEST_TMPDIR/xattr-xrdfs-virtual.out"
    local xrdfs_native="$BATS_TEST_TMPDIR/xattr-xrdfs-native.out"

    run record_combined "$gfal_virtual" gfal-xattr "$TEST_REGULAR"
    assert_success
    run record_combined "$xrdfs_virtual" \
        "$XRDFS" xattr "$TEST_REGULAR"
    assert_success
    for attribute in xroot.cksum xroot.space xroot.xattr spacetoken; do
        run grep -F "$attribute" "$gfal_virtual"
        assert_success
        run grep -F "$attribute" "$xrdfs_virtual"
        assert_success
    done
    run grep -F user.fixture "$gfal_virtual"
    assert_failure
    run grep -F user.fixture "$xrdfs_virtual"
    assert_failure

    run record "$xrdfs_native" "$xrdfs_native.err" \
        "$XRDFS" xattr "$TEST_REGULAR" list
    assert_success
    run grep -F 'user.fixture="fixture"' "$xrdfs_native"
    assert_success
    run "$XRDFS" xattr "$TEST_REGULAR" get user.fixture
    assert_success
    assert_output --partial '# file: /data/regular.txt'
    assert_output --partial 'user.fixture="fixture"'
}

@test "reference copy: remote-to-local copy, overwrite, and checksum agree" {
    local gfal_dst="$BATS_TEST_TMPDIR/copy-gfal.dat"
    local xrdfs_dst="$BATS_TEST_TMPDIR/copy-xrdcp.dat"

    run gfal-copy "$TEST_ALL_BYTES" "file://$gfal_dst"
    assert_success
    run "$XRDCP" --silent "$TEST_ALL_BYTES" "file://$xrdfs_dst"
    assert_success
    run cmp "$gfal_dst" "$xrdfs_dst"
    assert_success

    printf stale >"$gfal_dst"
    printf stale >"$xrdfs_dst"
    run gfal-copy -f -K ADLER32 "$TEST_ALL_BYTES" "file://$gfal_dst"
    assert_success
    run "$XRDCP" --silent -f --rm-bad-cksum -C adler32 \
        "$TEST_ALL_BYTES" "file://$xrdfs_dst"
    assert_success
    run cmp "$gfal_dst" "$xrdfs_dst"
    assert_success
    run cmp "$gfal_dst" \
        "$BATS_TEST_TMPDIR/xrdfs-gfal2-reference/data/all-bytes.dat"
    assert_success
}

@test "reference copy: missing sources fail without leaving local destinations" {
    local gfal_dst="$BATS_TEST_TMPDIR/copy-missing-gfal.dat"
    local xrdcp_dst="$BATS_TEST_TMPDIR/copy-missing-xrdcp.dat"

    run gfal-copy "$TEST_MISSING" "file://$gfal_dst"
    assert_failure
    run "$XRDCP" --silent "$TEST_MISSING" "$xrdcp_dst"
    assert_failure
    run test ! -e "$gfal_dst"
    assert_success
    run test ! -e "$xrdcp_dst"
    assert_success
}

@test "reference copy: existing destinations fail without force and remain unchanged" {
    local gfal_dst="$BATS_TEST_TMPDIR/copy-existing-gfal.dat"
    local xrdcp_dst="$BATS_TEST_TMPDIR/copy-existing-xrdcp.dat"
    printf stale >"$gfal_dst"
    printf stale >"$xrdcp_dst"

    run gfal-copy "$TEST_REGULAR" "file://$gfal_dst"
    assert_failure
    run "$XRDCP" --silent "$TEST_REGULAR" "file://$xrdcp_dst"
    assert_failure
    run test "$(cat "$gfal_dst")" = stale
    assert_success
    run test "$(cat "$xrdcp_dst")" = stale
    assert_success
}

@test "reference copy: parent and stream options map without changing bytes" {
    local gfal_dst="$BATS_TEST_TMPDIR/gfal-parent/nested/output.dat"
    local xrdcp_dst="$BATS_TEST_TMPDIR/xrdcp-parent/nested/output.dat"

    run gfal-copy -p -n 2 "$TEST_ALL_BYTES" "file://$gfal_dst"
    assert_success
    run "$XRDCP" --silent -p -S 2 \
        "$TEST_ALL_BYTES" "file://$xrdcp_dst"
    assert_success
    run cmp "$gfal_dst" "$xrdcp_dst"
    assert_success
}

@test "reference copy: stdout is an xrdcp superset and checksum failures clean up" {
    local gfal_stdout="$BATS_TEST_TMPDIR/copy-gfal-stdout.out"
    local xrdfs_stdout="$BATS_TEST_TMPDIR/copy-xrdcp-stdout.out"
    local xrdcp_bad="$BATS_TEST_TMPDIR/copy-xrdcp-bad.dat"
    local xrdcp_kept="$BATS_TEST_TMPDIR/copy-xrdcp-kept.dat"

    # Legacy gfal-copy does not treat '-' as stdout. xrdcp does, so this is a
    # documented native superset rather than a parity failure.
    run record "$gfal_stdout" "$gfal_stdout.err" \
        gfal-copy "$TEST_REGULAR" -
    assert_failure
    run record "$xrdfs_stdout" "$xrdfs_stdout.err" \
        "$XRDCP" --silent "$TEST_REGULAR" -
    assert_success
    run cmp "$xrdfs_stdout" \
        "$BATS_TEST_TMPDIR/xrdfs-gfal2-reference/data/regular.txt"
    assert_success

    # A deliberately wrong expected value exercises xrdcp's cleanup contract.
    # gfal2-util does not provide a portable way to inject a wrong source-side
    # checksum into this otherwise healthy local reference server.
    run "$XRDCP" --silent -C adler32:00000000 --rm-bad-cksum \
        "$TEST_REGULAR" "$xrdcp_bad"
    assert_failure
    run test ! -e "$xrdcp_bad"
    assert_success

    # Without the mapped cleanup flag, xrdcp intentionally leaves the bad
    # local destination. This guards why --rm-bad-cksum is part of the mapping.
    run "$XRDCP" --silent -C adler32:00000000 \
        "$TEST_REGULAR" "$xrdcp_kept"
    assert_failure
    run test -e "$xrdcp_kept"
    assert_success
}

@test "reference copy: --from-file maps to xrdcp --infiles for local downloads" {
    local sources="$BATS_TEST_TMPDIR/copy-sources.txt"
    local gfal_dir="$BATS_TEST_TMPDIR/copy-gfal-dir"
    local xrdcp_dir="$BATS_TEST_TMPDIR/copy-xrdcp-dir"
    mkdir "$gfal_dir" "$xrdcp_dir"
    printf '\n%s\n\n%s\n\n' "$TEST_REGULAR" "$TEST_SECOND" >"$sources"

    run gfal-copy --from-file "$sources" "file://$gfal_dir/"
    assert_success
    run "$XRDCP" --silent --infiles "$sources" "file://$xrdcp_dir/"
    assert_success
    run cmp "$gfal_dir/regular.txt" "$xrdcp_dir/regular.txt"
    assert_success
    run cmp "$gfal_dir/second.txt" "$xrdcp_dir/second.txt"
    assert_success
}

@test "reference copy: input-list failures retain each tool's native policy" {
    local sources="$BATS_TEST_TMPDIR/copy-mixed-sources.txt"
    local gfal_dir="$BATS_TEST_TMPDIR/copy-mixed-gfal"
    local xrdcp_dir="$BATS_TEST_TMPDIR/copy-mixed-xrdcp"
    mkdir "$gfal_dir" "$xrdcp_dir"
    printf '%s\n%s\n%s\n' \
        "$TEST_REGULAR" "$TEST_MISSING" "$TEST_SECOND" >"$sources"

    run gfal-copy --from-file "$sources" "file://$gfal_dir/"
    assert_failure
    run "$XRDCP" --silent --infiles "$sources" "file://$xrdcp_dir/"
    assert_failure
    run test -f "$gfal_dir/regular.txt"
    assert_success
    run test ! -e "$gfal_dir/second.txt"
    assert_success
    run test -f "$xrdcp_dir/regular.txt"
    assert_success
    run test -f "$xrdcp_dir/second.txt"
    assert_success
}

@test "reference copy: recursive destination layout and empty directories remain distinct" {
    local gfal_dir="$BATS_TEST_TMPDIR/tree-gfal"
    local xrdcp_missing="$BATS_TEST_TMPDIR/tree-xrdcp-missing"
    local xrdcp_dir="$BATS_TEST_TMPDIR/tree-xrdcp"

    run gfal-copy -r "$TEST_TREE" "file://$gfal_dir"
    assert_success
    run test -f "$gfal_dir/nested/leaf.txt"
    assert_success
    run test -f "$gfal_dir/.tree-hidden"
    assert_success
    run test -d "$gfal_dir/empty-child"
    assert_success

    # xrdcp requires an existing directory for this form, nests the remote
    # basename below it, and represents files rather than empty directories.
    run "$XRDCP" --silent -r "$TEST_TREE" "$xrdcp_missing"
    assert_failure
    run test ! -e "$xrdcp_missing"
    assert_success

    mkdir "$xrdcp_dir"
    run "$XRDCP" --silent -r "$TEST_TREE" "$xrdcp_dir/"
    assert_success
    run test -f "$xrdcp_dir/tree/nested/leaf.txt"
    assert_success
    run test -f "$xrdcp_dir/tree/.tree-hidden"
    assert_success
    run test ! -d "$xrdcp_dir/tree/empty-child"
    assert_success
}

@test "live reference: ROOT, HTTPS, and DAVS file reads agree" {
    require_live_reference_files

    local davs_file=${XRDFS_GFAL2_LIVE_HTTPS_FILE/#https:\/\//davs://}
    local name url gfal_out xrdfs_out gfal_copy xrdcp_copy
    for name in root https davs; do
        case "$name" in
            root) url=$XRDFS_GFAL2_LIVE_ROOT_FILE ;;
            https) url=$XRDFS_GFAL2_LIVE_HTTPS_FILE ;;
            davs) url=$davs_file ;;
        esac
        gfal_out="$BATS_TEST_TMPDIR/live-$name-gfal.out"
        xrdfs_out="$BATS_TEST_TMPDIR/live-$name-xrdfs.out"

        run record "$gfal_out" "$gfal_out.err" gfal-stat "$url"
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" stat "$url"
        assert_success
        run test "$(stat_size "$gfal_out")" = "$(stat_size "$xrdfs_out")"
        assert_success

        run record "$gfal_out" "$gfal_out.err" gfal-cat -b "$url"
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" cat -b "$url"
        assert_success
        run cmp "$gfal_out" "$xrdfs_out"
        assert_success

        run record "$gfal_out" "$gfal_out.err" gfal-sum "$url" ADLER32
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" \
            "$XRDFS" sum "$url" ADLER32
        assert_success
        run test "$(checksum_digest "$gfal_out")" = \
            "$(checksum_digest "$xrdfs_out")"
        assert_success

        run record "$gfal_out" "$gfal_out.err" \
            gfal-xattr "$url" user.checksum.adler32
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" \
            "$XRDFS" xattr "$url" user.checksum.adler32
        assert_success
        run test "$(tr '[:upper:]' '[:lower:]' <"$gfal_out")" = \
            "$(tr '[:upper:]' '[:lower:]' <"$xrdfs_out")"
        assert_success

        run record "$gfal_out" "$gfal_out.err" gfal-ls -l \
            --xattr user.checksum.adler32 "$url"
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" ls -l \
            --xattr user.checksum.adler32 "$url"
        assert_success
        run test "$(cut -f2 "$gfal_out")" = "$(cut -f2 "$xrdfs_out")"
        assert_success

        gfal_copy="$BATS_TEST_TMPDIR/live-$name-gfal.copy"
        xrdcp_copy="$BATS_TEST_TMPDIR/live-$name-xrdcp.copy"
        run gfal-copy -K ADLER32 "$url" "file://$gfal_copy"
        assert_success
        run "$XRDCP" --silent --rm-bad-cksum -C adler32 \
            "$url" "$xrdcp_copy"
        assert_success
        run cmp "$gfal_copy" "$xrdcp_copy"
        assert_success
    done
}

@test "live reference: ROOT, HTTPS, and DAVS directory reads agree" {
    require_live_reference_directories

    local davs_dir=${XRDFS_GFAL2_LIVE_HTTPS_DIR/#https:\/\//davs://}
    local name url gfal_out xrdfs_out
    for name in root https davs; do
        case "$name" in
            root) url=$XRDFS_GFAL2_LIVE_ROOT_DIR ;;
            https) url=$XRDFS_GFAL2_LIVE_HTTPS_DIR ;;
            davs) url=$davs_dir ;;
        esac
        gfal_out="$BATS_TEST_TMPDIR/live-dir-$name-gfal.out"
        xrdfs_out="$BATS_TEST_TMPDIR/live-dir-$name-xrdfs.out"

        run record "$gfal_out" "$gfal_out.err" gfal-stat "$url"
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" stat "$url"
        assert_success
        run grep -Eiq 'directory' "$gfal_out"
        assert_success
        run grep -F 'IsDir' "$xrdfs_out"
        assert_success

        run record "$gfal_out" "$gfal_out.err" gfal-ls "$url"
        assert_success
        run record "$xrdfs_out" "$xrdfs_out.err" "$XRDFS" ls "$url"
        assert_success
        normalize_listing "$gfal_out" >"$gfal_out.names"
        normalize_listing "$xrdfs_out" >"$xrdfs_out.names"
        run cmp "$gfal_out.names" "$xrdfs_out.names"
        assert_success
    done
}
