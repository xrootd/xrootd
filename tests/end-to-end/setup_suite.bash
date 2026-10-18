#!/usr/bin/env bash

load "$(dirname $(realpath ${BASH_SOURCE[0]}))/helper/tls.bash"

# When run from a CMake build, the tests must use the programs of that build,
# rather than the ones of an XRootD installation on the system.
check_build_programs() {
    local prog

    [[ -n "${BINARY_DIR:-}" ]] || return 0

    for prog in xrootd cmsd xrdcp xrdfs; do
        if [[ "$(PATH="${BINARY_DIR}/bin:${PATH}" command -v "$prog")" != "${BINARY_DIR}/bin/$prog" ]]; then
            echo "'$prog' does not come from ${BINARY_DIR}/bin" >&2
            return 1
        fi
    done
}

setup_suite() {
    check_build_programs
    generate_tls
}
