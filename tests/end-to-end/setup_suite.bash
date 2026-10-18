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

# Start setup/reaper.sh, which stops the daemons of the tests when BATS exits
# without its teardown, for example when CTest kills it at a timeout. The
# double fork puts the reaper outside of the process tree of BATS, and the
# redirections free the descriptors which BATS waits for: 0 to 2, and 3 and 4,
# which BATS opens on the output of the suite. The reaper waits for this
# process, bats-exec-suite, and for its parent, the main process of BATS,
# which removes the run directory when it exits.
start_reaper() {
    ( "$(dirname "$(realpath "${BASH_SOURCE[0]}")")/setup/reaper.sh" \
        "$PPID $$" "$BATS_RUN_TMPDIR" "${BATS_TEMPDIR_CLEANUP:-}" < /dev/null > /dev/null 2>&1 3>&- 4>&- & )
}

setup_suite() {
    start_reaper
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
