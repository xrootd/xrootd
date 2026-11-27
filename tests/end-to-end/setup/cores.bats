#!/usr/bin/env bash

# Tests of the functions which print the backtraces of crashes (see
# tests/print-cores.sh). They start no server.

load server.bash

# Write a core: with gdb, of a shell which crashes with SIGSEGV, or with
# lldb, of a program of the build at its entry, as macOS does not let a
# debugger run its own programs, such as sh. The debugger writes the core, as
# the kernel setting can keep the cores of the kernel outside of the test.
make_core() {
    if command -v gdb > /dev/null; then
        gdb -q -batch -ex run -ex "generate-core-file $1" \
            --args sh -c 'kill -SEGV $$' > /dev/null 2>&1
    elif command -v lldb > /dev/null; then
        lldb --batch -o 'process launch --stop-at-entry' -o "process save-core $1" \
            -- "$(command -v xrdfs)" > /dev/null 2>&1
    fi
    [[ -s "$1" ]]
}

# Write a core with make_core, or skip the test. lldb needs the permission
# to debug, which a session without a terminal does not get on macOS until
# DevToolsSecurity enables it.
require_core() {
    make_core "$1" || skip "no debugger which can write a core"
}

@test "print_cores prints the backtrace of a core in the test directory" {
    require_core "$BATS_TEST_TMPDIR/core.1"
    SETUP_CORE_PATTERN=core run -0 print_cores
    assert_line "[core] $BATS_TEST_TMPDIR/core.1"
    assert_line --regexp '^\[core\] .*#0[: ] '
}

@test "print_cores prints the backtrace of a core in the directory of the pattern" {
    mkdir "$BATS_TEST_TMPDIR/cores"
    require_core "$BATS_TEST_TMPDIR/cores/core.sh.1"
    SETUP_CORE_PATTERN="$BATS_TEST_TMPDIR/cores/core.%e.%p" run -0 print_cores
    assert_line "[core] $BATS_TEST_TMPDIR/cores/core.sh.1"
    assert_line --regexp '^\[core\] .*#0[: ] '
}

# The header of a 64-bit Mach-O file: the magic number, the CPU type and
# subtype, the file type MH_CORE, all little-endian, and no load commands,
# then the directory of the run of BATS, which the environment of a program
# of the run has. lldb reads the number of load commands from the header, and
# without the zeros it takes the directory for a very large number.
@test "print_cores finds a core of macOS" {
    local header='\xcf\xfa\xed\xfe\x0c\x00\x00\x01\x00\x00\x00\x00\x04\x00\x00\x00'
    local zeros='\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'

    printf "$header$zeros%s" "$BATS_RUN_TMPDIR" > "$BATS_TEST_TMPDIR/core.2"
    SETUP_CORE_PATTERN=core run -0 print_cores
    assert_line "[core] $BATS_TEST_TMPDIR/core.2"
}

@test "print_cores prints no file which is not a core" {
    echo 'not a core' > "$BATS_TEST_TMPDIR/core.txt"
    SETUP_CORE_PATTERN=core run -0 print_cores
    assert_output ''
}

@test "print_cores prints no core of an earlier test" {
    require_core "$BATS_TEST_TMPDIR/core.1"
    touch -t 200001010000 "$BATS_TEST_TMPDIR/core.1"
    SETUP_CORE_PATTERN=core run -0 print_cores
    assert_output ''
}

# A program of another run of BATS does not have the directory of this run in
# its environment
@test "print_cores prints no core of a program of another run" {
    require_core "$BATS_TEST_TMPDIR/probe"
    env -i PATH="$PATH" bash -c "$(declare -f make_core); make_core '$BATS_TEST_TMPDIR/core.1'"
    SETUP_CORE_PATTERN=core run -0 print_cores
    assert_output ''
    [[ -s "$BATS_TEST_TMPDIR/core.1" ]]
}

@test "print_cores prints nothing for a pipe to another handler" {
    SETUP_CORE_PATTERN='|/usr/share/apport/apport %p' run -0 print_cores
    assert_output ''
}

PRINT_CORES="$BATS_TEST_DIRNAME/../../print-cores.sh"

@test "print-cores.sh prints the backtrace of a core of a command which fails" {
    require_core "$BATS_TEST_TMPDIR/probe"
    export -f make_core
    cd "$BATS_TEST_TMPDIR"
    SETUP_CORE_PATTERN=core run -3 "$PRINT_CORES" bash -c 'make_core core.1; exit 3'
    assert_line "[core] $BATS_TEST_TMPDIR/core.1"
    assert_line --regexp '^\[core\] .*#0[: ] '
}

@test "print-cores.sh prints no core of a program of another command" {
    require_core "$BATS_TEST_TMPDIR/probe"
    export -f make_core
    cd "$BATS_TEST_TMPDIR"
    SETUP_CORE_PATTERN=core run -3 "$PRINT_CORES" bash -c \
        'env -u PRINT_CORES_TAG bash -c "make_core core.1"; exit 3'
    refute_output --partial '[core]'
    [[ -s "$BATS_TEST_TMPDIR/core.1" ]]
}

@test "print-cores.sh prints no core of a command which succeeds" {
    require_core "$BATS_TEST_TMPDIR/probe"
    export -f make_core
    cd "$BATS_TEST_TMPDIR"
    SETUP_CORE_PATTERN=core run -0 "$PRINT_CORES" bash -c 'make_core core.1'
    assert_output ''
}

@test "print-cores.sh prints no core of an earlier command" {
    require_core "$BATS_TEST_TMPDIR/core.1"
    touch -t 200001010000 "$BATS_TEST_TMPDIR/core.1"
    cd "$BATS_TEST_TMPDIR"
    SETUP_CORE_PATTERN=core run -1 "$PRINT_CORES" false
    assert_output ''
}

@test "print-cores.sh ends with the signal which ended the command" {
    SETUP_CORE_PATTERN=core run -143 "$PRINT_CORES" sh -c 'kill -TERM $$'
    refute_output --partial '[core]'
}
