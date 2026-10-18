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

[[ -d "$dir" ]] || exit 0

# A daemon of a test has the run directory in its command line; a pid file
# can name a process which has reused the pid
daemon_of_run() {
    ps -p "$1" -o command= 2>/dev/null | grep -qF "$dir"
}

pids=()
while IFS= read -r -d '' pidfile; do
    p=$(<"$pidfile")
    [[ "$p" =~ ^[0-9]+$ ]] || continue
    if daemon_of_run "$p"; then
        kill -TERM "$p" 2>/dev/null && pids+=("$p")
    fi
done < <(find "$dir" -name '*.pid' -print0 2>/dev/null)

for _ in {1..50}; do
    alive=0
    for p in "${pids[@]}"; do
        kill -0 "$p" 2>/dev/null && alive=1
    done
    (( alive )) || break
    sleep 0.1
done

for p in "${pids[@]}"; do
    kill -KILL "$p" 2>/dev/null
done

while IFS= read -r -d '' link; do
    target=$(readlink "$link")
    [[ "$target" == */xrd.* ]] && rm -rf "$target"
done < <(find "$dir" -type l -name admin -print0 2>/dev/null)

if [[ -n "$cleanup" ]]; then
    chmod -R u+w "$dir" 2>/dev/null
    rm -rf "$dir"
fi
