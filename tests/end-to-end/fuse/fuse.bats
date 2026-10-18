#!/usr/bin/env bash

# The FUSE file system xrootdfs, which mounts the server below the directory of
# the test file. The tests share the server and the mount; each test works in
# its own directory of the mount.

load ../setup/server.bash

export XROOTDFS_NO_ALLOW_OTHER=1

setup_file() {
    require_commands xrootdfs fusermount
    [[ -w /dev/fuse ]] || skip 'no FUSE device'

    start_server server server

    export MOUNT="$BATS_FILE_TMPDIR/mnt"
    mkdir "$MOUNT"
    # xrootdfs needs two slashes after the host. It must not keep the
    # descriptors 3 and 4 of BATS, else BATS waits for it.
    xrootdfs -o rdr="$SERVER_URL//" "$MOUNT" 3>&- 4>&-
}

# xrootdfs has no pid file, thus find it by the mount point. Stop it after
# the unmount, also when the unmount fails, before stop_servers checks for
# processes which survived.
teardown_file() {
    local pids pid
    if [[ -n "${MOUNT:-}" ]]; then
        pids=$(pgrep -f -- "$MOUNT" || true)
        fusermount -u "$MOUNT" || fusermount -uz "$MOUNT" || true
        for pid in $pids; do
            stop_pid "$pid" || true
        done
    fi
    stop_servers
}

setup() {
    D="t${BATS_TEST_NUMBER}"
    mkdir "$MOUNT/$D"
    cd "$BATS_TEST_TMPDIR"
    make_random_file local.dat 100003
}

@test "the mount is a FUSE mount" {
    run -0 grep -F " $MOUNT fuse" /proc/mounts
}

@test "a file copied into the mount is on the server" {
    cp local.dat "$MOUNT/$D/copy.dat"
    run -0 cmp local.dat "$SERVER_DATA/$D/copy.dat"
}

@test "a file copied out of the mount keeps the content" {
    cp local.dat "$SERVER_DATA/$D/remote.dat"
    cp "$MOUNT/$D/remote.dat" copy.dat
    run -0 cmp local.dat copy.dat
}

@test "a copy in the mount keeps the checksum" {
    cp local.dat "$MOUNT/$D/a.dat"
    cp "$MOUNT/$D/a.dat" "$MOUNT/$D/b.dat"
    assert_equal "$(checksum crc32c "$MOUNT/$D/b.dat")" "$(checksum crc32c local.dat)"
}

@test "ls -l of the mount lists a file" {
    cp local.dat "$MOUNT/$D/file.dat"
    run -0 ls -l "$MOUNT/$D"
    assert_output --partial 'file.dat'
}

@test "mv in the mount renames the file on the server" {
    cp local.dat "$MOUNT/$D/old.dat"
    mv "$MOUNT/$D/old.dat" "$MOUNT/$D/new.dat"
    run -0 test -f "$SERVER_DATA/$D/new.dat"
}

@test "rm in the mount removes the file on the server" {
    cp local.dat "$MOUNT/$D/file.dat"
    rm "$MOUNT/$D/file.dat"
    run ! test -e "$SERVER_DATA/$D/file.dat"
}

@test "mkdir -p in the mount creates the directories on the server" {
    mkdir -p "$MOUNT/$D/a/b/c"
    run -0 test -d "$SERVER_DATA/$D/a/b/c"
}

@test "rm -rf in the mount removes a tree of many files" {
    for dir in $(seq 16); do
        mkdir -p "$MOUNT/$D/tree/$dir/a/b"
        for file in $(seq 16); do
            echo "$dir $file" > "$MOUNT/$D/tree/$dir/a/b/$file"
        done
    done
    rm -rf "$MOUNT/$D/tree"
    run ! test -e "$SERVER_DATA/$D/tree"
}
