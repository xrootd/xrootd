#!/usr/bin/env bash

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

# CTest creates the shared credentials once for all tests (the fixture
# "credentials", see setup/credentials.sh) and sets CREDENTIALS. A run of bats
# without CTest creates them for its own tests.
check_credentials() {
    if [[ -r "${CREDENTIALS:-}/ca.pem" ]]; then
        return 0
    fi

    export CREDENTIALS="$BATS_SUITE_TMPDIR/credentials"
    PATH="${BINARY_DIR:+${BINARY_DIR}/bin:}${PATH}" \
        "$(dirname "$(realpath "${BASH_SOURCE[0]}")")/setup/credentials.sh" create "$CREDENTIALS"
}

setup_suite() {
    check_build_programs
    check_credentials
}

# The credentials are read-only, which BATS cannot remove, thus remove the
# ones which check_credentials created.
teardown_suite() {
    if [[ "${CREDENTIALS:-}" == "$BATS_SUITE_TMPDIR/credentials" ]]; then
        "$(dirname "$(realpath "${BASH_SOURCE[0]}")")/setup/credentials.sh" remove "$CREDENTIALS"
    fi
}
