#!/usr/bin/env bash

# The cache of file locations of the managers. A data server tells its
# managers with a "have" message when it makes a file or a directory, and
# with a "gone" message when it removes one. A manager forwards both messages
# to its own managers. The messages arrive later than the answer to the
# client, thus the cache can be older than the last change of a path. The
# data server, not the cache, must then decide whether a path exists. The
# many clients at once also check the selection of a node, which runs for
# several clients at the same time.
#
# A cluster of a meta manager and one manager, with two data servers below
# the manager. The tests share the cluster; each test has its own paths.

load ../setup/cluster.bash

# Each client makes and removes its own path CYCLES times, each cycle right
# after the one before. Many clients at once keep the managers busy, thus a
# client often makes its path again before the "gone" of its last remove
# reaches a manager.
CLIENTS=8
CYCLES=200

setup_file() {
    require_commands xrdcl-api
    start_meta_manager metaman
    start_manager man1 metaman
    start_data_server srv1 man1
    start_data_server srv2 man1
    wait_for_cluster metaman 2
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    P="/cache${BATS_TEST_NUMBER}"
}

# recreate_all <url> [dir]
#
# Run CLIENTS clients of "xrdcl-api recreate" at once, client n on the path
# <url>-<n>. Print the result of each client, and fail when one fails.
recreate_all() {
    local url=$1 n pids=() rc=0
    local options=("${@:2}")

    for (( n = 1; n <= CLIENTS; n++ )); do
        xrdcl-api recreate "$url-$n" "$CYCLES" "${options[@]}" > "client$n.out" 2>&1 &
        pids+=("$!")
    done

    for (( n = 1; n <= CLIENTS; n++ )); do
        wait "${pids[n - 1]}" || rc=1
        echo "client $n: $(cat "client$n.out")"
    done

    return "$rc"
}

@test "files which clients make and remove at once through a manager can be made again" {
    run -0 recreate_all "$MAN1_URL/$P"
    assert_line "client 1: $CYCLES cycles"
}

@test "files which clients make and remove at once through a meta manager can be made again" {
    run -0 recreate_all "$METAMAN_URL/$P"
    assert_line "client 1: $CYCLES cycles"
}

@test "directories which clients make and remove at once through a manager can be made again" {
    run -0 recreate_all "$MAN1_URL/$P" dir
    assert_line "client 1: $CYCLES cycles"
}

@test "directories which clients make and remove at once through a meta manager can be made again" {
    run -0 recreate_all "$METAMAN_URL/$P" dir
    assert_line "client 1: $CYCLES cycles"
}

@test "a mkdir -p of a directory which exists succeeds through a manager" {
    xrdfs "$MAN1_URL" mkdir -p "$P/sub"
    run -0 xrdfs "$MAN1_URL" mkdir -p "$P/sub"
}

@test "a mkdir -p of a directory which exists succeeds through a meta manager" {
    xrdfs "$METAMAN_URL" mkdir -p "$P/sub"
    run -0 xrdfs "$METAMAN_URL" mkdir -p "$P/sub"
}

@test "a copy without --force to a file which exists fails through a manager" {
    echo data > local.dat
    xrdcp local.dat "$MAN1_URL/$P.dat"
    run -54 xrdcp local.dat "$MAN1_URL/$P.dat"
    assert_output --partial '[3018]'
}

@test "a copy without --force to a file which exists fails through a meta manager" {
    echo data > local.dat
    xrdcp local.dat "$METAMAN_URL/$P.dat"
    run -54 xrdcp local.dat "$METAMAN_URL/$P.dat"
    assert_output --partial '[3018]'
}
