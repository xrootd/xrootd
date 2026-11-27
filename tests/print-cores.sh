#!/usr/bin/env bash

# print-cores.sh <command> [<argument>...]
#
# Run the command of a test. When it fails, print the backtraces of its
# programs which crashed. test.cmake makes this script the launcher of the
# tests (CMAKE_TEST_LAUNCHER). helper/common.bash of the end-to-end tests
# loads this file for its functions.

# allow_cores
#
# Let each program which crashes write a core file, up to the hard limit of
# its size. A sanitizer on a 64-bit system sets the limit to one byte, unless
# disable_coredump=0. The core does not contain the shadow memory.
allow_cores() {
    local var

    ulimit -S -c "$(ulimit -H -c)" 2>/dev/null || true
    for var in ASAN_OPTIONS HWASAN_OPTIONS LSAN_OPTIONS MSAN_OPTIONS TSAN_OPTIONS; do
        [[ "${!var:-}" == *disable_coredump=* ]] && continue
        export "$var=${!var:+${!var}:}disable_coredump=0"
    done
}

# Print the time of the last change of a file, in seconds since the epoch.
# GNU and BusyBox stat take -c, the stat of macOS and BSD takes -f.
_mtime() {
    stat -c %Y "$1" 2>/dev/null || stat -f %m "$1"
}

# Read <count> bytes at <offset> of a file, in hexadecimal
_bytes() {
    od -An -tx1 -j"$2" -N"$3" "$1" 2>/dev/null | tr -d ' \n'
}

# Return success if the file is an ELF core file (Linux) of either byte
# order, or a 64-bit Mach-O core file (macOS)
_is_core() {
    case "$(_bytes "$1" 0 4)" in
        7f454c46) case "$(_bytes "$1" 16 2)" in 0400|0004) ;; *) return 1 ;; esac ;;
        cffaedfe) [[ "$(_bytes "$1" 12 1)" == 04 ]] ;;
        *) return 1 ;;
    esac
}

# Print the backtrace of each thread of the program of a core file, with gdb,
# or else with lldb, which macOS has. Thread 1 crashed, and it comes first.
_print_core() {
    local core=$1 exe

    printf '\n[core] %s\n' "$core"
    if command -v gdb > /dev/null; then
        # An old gdb does not find the program of a core. The kernel cuts the
        # command line in the core, but AT_EXECFN has the full path.
        exe=$(gdb -nx -q -batch -iex 'set auto-solib-add off' -c "$core" \
                  -ex 'info auxv' 2>/dev/null |
              sed -n 's/.*AT_EXECFN.*"\(.*\)"$/\1/p')
        [[ -x "$exe" ]] || exe=

        gdb -nx -q -batch -ex 'thread apply all -ascending bt' \
            ${exe:+"$exe"} -c "$core" 2>&1 | sed 's/^/[core] /'
    elif command -v lldb > /dev/null; then
        # The core has the paths of the images of the program
        lldb --batch -c "$core" -o 'thread backtrace all' 2>&1 | sed 's/^/[core] /'
    else
        echo "[core] no gdb or lldb to print the backtrace"
    fi
}

# print_cores_since <marker> <tag> [<directory>...]
#
# Print the backtraces of the programs which crashed since the last change of
# the file <marker>, and which have the text <tag> in their memory, for
# example in their environment. An empty tag selects all the cores.
#
# The setting kernel.core_pattern (Linux) or kern.corefile (macOS) gives the
# place of the cores: an absolute path, a name in the working directory of
# the program, below one of the directories, or systemd-coredump. Another
# pipe, for example to apport, keeps no core. SETUP_CORE_PATTERN replaces the
# setting in the tests of this function.
print_cores_since() {
    local since=$1 tag=$2 pattern prefix core start pid
    local dirs=()

    shift 2
    [[ -e "$since" ]] || return 0
    pattern=${SETUP_CORE_PATTERN:-$(cat /proc/sys/kernel/core_pattern 2>/dev/null ||
                                    sysctl -n kern.corefile 2>/dev/null)} || return 0
    start=$(_mtime "$since") || return 0

    case "$pattern" in
        '|'*systemd-coredump*)
            # The journal has the environment of the program. Without --all,
            # journalctl prints null for a long field. The JSON of an old
            # journalctl has spaces around the colon.
            command -v coredumpctl > /dev/null || return 0
            journalctl --no-pager -q --all --since="@$start" -o json \
                --output-fields=COREDUMP_PID,COREDUMP_ENVIRON \
                MESSAGE_ID=fc2e22bc6ee647b6b90729ab34a250b1 \
                COREDUMP_UID="$(id -u)" 2>/dev/null | grep -F -- "$tag" |
                sed -n 's/.*"COREDUMP_PID" *: *"\([0-9]*\)".*/\1/p' |
                while read -r pid; do
                    coredumpctl --no-pager --since="@$start" info "$pid" 2>/dev/null |
                        sed 's/^/[core] /'
                done
            return 0 ;;
        '|'*|'')
            return 0 ;;
        /*)
            dirs=("$(dirname "$pattern")") ;;
        *)
            (( $# > 0 )) || return 0
            dirs=("$@") ;;
    esac

    # The literal part of the name, before the first specifier, as in core.%p
    prefix=$(basename "$pattern")
    prefix=${prefix%%%*}

    # find -newer of BusyBox compares whole seconds, thus it misses a core of
    # the second of the start, so compare the times here
    while IFS= read -r -d '' core; do
        (( $(_mtime "$core") >= start )) || continue
        _is_core "$core" || continue
        [[ -z "$tag" ]] || grep -qaF -- "$tag" "$core" || continue
        _print_core "$core"
    done < <(find "${dirs[@]}" -type f -name "${prefix}*" -print0 2>/dev/null)
    return 0
}

# Run the command, and print the cores of its crashes when it fails. Each
# program of the command inherits PRINT_CORES_TAG, a unique path, thus the
# cores of another test which runs at the same time do not show. A signal
# which ended the command also ends this shell, thus CTest shows it, for
# example SegFault.
_run_with_cores() {
    local marker status signal

    allow_cores
    marker=$(mktemp "${TMPDIR:-/tmp}/core-since.XXXXXX") || exec "$@"

    PRINT_CORES_TAG=$marker "$@"
    status=$?

    if (( status != 0 )); then
        print_cores_since "$marker" "PRINT_CORES_TAG=$marker" "$PWD"
    fi
    rm -f "$marker"

    signal=$( (( status > 128 )) && kill -l "$status" 2>/dev/null)
    case "$signal" in
        ABRT|ALRM|BUS|FPE|HUP|ILL|INT|KILL|PIPE|QUIT|SEGV|SYS|TERM|TRAP|USR1|USR2|XCPU|XFSZ)
            ulimit -c 0 2>/dev/null
            trap - "$signal"
            kill -s "$signal" "$$" ;;
    esac
    exit "$status"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    _run_with_cores "$@"
fi
