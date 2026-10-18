#!/usr/bin/env bash

# reaper.sh <pids> <run dir> <cleanup>
#
# Wait until all processes <pids> of BATS (a list separated by spaces) exit,
# then stop the daemons which the tests left below <run dir>, remove their
# admin directories outside of it, and, if <cleanup> is not empty, remove
# <run dir>.
#
# The teardown of the tests stops the daemons. When CTest kills BATS at a
# timeout, the teardown does not run, and the daemons, which run in the
# background, survive BATS. setup_suite starts this script outside of the
# process tree of BATS, thus it survives too.

bats_pids=$1 dir=$2 cleanup=$3

# BATS removes <run dir> itself when its main process exits. A second removal
# at the same time fails where rm -rf reports files which are gone (BusyBox),
# and the failure makes BATS fail, thus wait for the main process too.
bats_runs() {
    local p
    for p in $bats_pids; do
        kill -0 "$p" 2>/dev/null && return 0
    done
    return 1
}

while bats_runs; do
    sleep 1
done

# A FUSE mount of a test (fuse/fuse.bats) becomes a dead mount when its
# daemon stops, and rm -rf cannot remove it. Unmount the mounts below the
# run directory first, lazily, as a process can still use them. Linux lists
# the mounts in /proc/mounts, macOS with mount.
mounts=$( { if [[ -r /proc/mounts ]]; then
                awk '{ print $2 }' /proc/mounts
            else
                mount | sed -n 's/^.* on \(.*\) (.*$/\1/p'
            fi; } | grep -F -- "$dir/" || true)

for m in $mounts; do
    fusermount -uz "$m" 2>/dev/null || umount -f "$m" 2>/dev/null
done

# A daemon of a test has the run directory in its command line. After a
# normal exit, BATS has removed the run directory with the pid files, and
# only a daemon which a test did not stop is left.
pids=$(pgrep -f -- "$dir/")

for p in $pids; do
    kill -TERM "$p" 2>/dev/null
done

for _ in {1..50}; do
    alive=0
    for p in $pids; do
        kill -0 "$p" 2>/dev/null && alive=1
    done
    (( alive )) || break
    sleep 0.1
done

for p in $pids; do
    kill -KILL "$p" 2>/dev/null
done

[[ -d "$dir" ]] || exit 0

while IFS= read -r -d '' link; do
    target=$(readlink "$link")
    [[ "$target" == */xrd.* ]] && rm -rf "$target"
done < <(find "$dir" -type l -name admin -print0 2>/dev/null)

if [[ -n "$cleanup" ]]; then
    chmod -R u+w "$dir" 2>/dev/null
    rm -rf "$dir"
fi
