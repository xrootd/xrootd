#!/usr/bin/env bash

# HTTP-TPC against one manager (man1) in front of two data servers (srv1, srv2).
# What this adds over tests/TPCTests is that either end of a COPY can be named by
# the redirector rather than by the server that actually holds the file.

set -Eexo pipefail

: "${XRDCP:=$(command -v xrdcp)}"
: "${XRDFS:=$(command -v xrdfs)}"
: "${OPENSSL:=$(command -v openssl)}"
: "${CURL:=$(command -v curl)}"

: "${HOST_MAN1:=http://localhost:10990}"
: "${HOST_SRV1:=http://localhost:10991}"
: "${HOST_SRV2:=http://localhost:10992}"

: "${XRD_SRV1:=${HOST_SRV1/http:/root:}}"
: "${XRD_SRV2:=${HOST_SRV2/http:/root:}}"

for PROG in "${XRDCP}" "${XRDFS}" "${OPENSSL}" "${CURL}"; do
    if [[ ! -x "${PROG}" ]]; then
        echo >&2 "$(basename "$0"): error: '${PROG}': command not found"
        exit 1
    fi
done

function error() {
    echo >&2 "$(basename "$0"): error: $*"
    exit 1
}

RMTDATADIR="/srvdata"
LCLDATADIR="${PWD}/localdata"
TPCBODY="${LCLDATADIR}/copy_body.txt"

mkdir -p "${LCLDATADIR}" "${PWD}/generated_tokens"
mkdir -p "${XDG_CACHE_HOME}/scitokens" && rm -rf "${XDG_CACHE_HOME:?}/scitokens"/*

# Named here so cleanup() can remove them however early the script dies.
# SELFFILE must stay on srv1 alone: the manager cases below rely on it having a
# single replica, so that a COPY naming the manager can only land back on srv1.
SELFFILE="${RMTDATADIR}/tpc_self.ref"
GOODFILE="${RMTDATADIR}/tpc_good.ref"
SAMEPATHFILE="${RMTDATADIR}/tpc_samepath.ref"
declare -a MADE=("${SELFFILE}" "${GOODFILE}" "${SAMEPATHFILE}"
                 "${RMTDATADIR}/tpc_pull_from_srv2.ref"
                 "${RMTDATADIR}/tpc_push_to_srv2.ref"
                 "${RMTDATADIR}/tpc_pull_from_man.ref"
                 "${RMTDATADIR}/tpc_push_to_man.ref"
                 "${RMTDATADIR}/tpc_pull_at_man.ref")

# Every path is removed from both servers: some were written through the manager,
# which picks the server itself, and without ofs.forward a rm sent to a manager
# only reaches one replica.
# shellcheck disable=SC2317
cleanup() {
    for path in "${MADE[@]}"; do
        ${XRDFS} "${XRD_SRV1}" rm "${path}" 2>/dev/null || :
        ${XRDFS} "${XRD_SRV2}" rm "${path}" 2>/dev/null || :
    done
    rm -rf "${LCLDATADIR}" || :
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 131' QUIT

setup_scitokens() {
    if ! ${XRDSCITOKENS_CREATE_TOKEN} \
        "${XRDSCITOKENS_ISSUER_DIR}"/issuer_pub_1.pem \
        "${XRDSCITOKENS_ISSUER_DIR}"/issuer_key_1.pem \
        test_1 \
        "https://localhost:7095/issuer/one" \
        "storage.modify:/ storage.create:/ storage.read:/" \
        1800 > "${PWD}/generated_tokens/token"; then
        error "failed to create token"
    fi
    chmod 0600 "${PWD}/generated_tokens/token"
}

setup_scitokens
export BEARER_TOKEN_FILE="${PWD}/generated_tokens/token"
BEARER_TOKEN=$(cat "${BEARER_TOKEN_FILE}")
export BEARER_TOKEN

# --location-trusted, not -L: curl drops Authorization when a redirect crosses to
# another host, and the manager redirects to a data server on another port.
CURLOPTS=(--location-trusted -s -S)

upload() {
    ${CURL} "${CURLOPTS[@]}" -H "Authorization: Bearer ${BEARER_TOKEN}" \
            -T "$2" "$1" -o /dev/null || error "upload to $1 failed"
}

download() {
    ${CURL} "${CURLOPTS[@]}" -H "Authorization: Bearer ${BEARER_TOKEN}" \
            "$1" -o "$2" || error "download of $1 failed"
}

# Echoes the status code, leaving the response body in ${TPCBODY}.
http_tpc() {
    local mode="$1" src="$2" dst="$3"

    case "${mode}" in
    pull) ${CURL} "${CURLOPTS[@]}" -X COPY -o "${TPCBODY}" -w "%{http_code}" \
                  -H "Authorization: Bearer ${BEARER_TOKEN}" \
                  -H "TransferHeaderAuthorization: Bearer ${BEARER_TOKEN}" \
                  -H "Source: ${src}" "${dst}" ;;
    push) ${CURL} "${CURLOPTS[@]}" -X COPY -o "${TPCBODY}" -w "%{http_code}" \
                  -H "Authorization: Bearer ${BEARER_TOKEN}" \
                  -H "TransferHeaderAuthorization: Bearer ${BEARER_TOKEN}" \
                  -H "Destination: ${dst}" "${src}" ;;
    *) error "unknown TPC mode: ${mode}" ;;
    esac
}

report_copy() {
    echo >&2 "----- COPY response body -----"
    cat >&2 "${TPCBODY}" || :
    echo >&2 "------------------------------"
}

assert_tpc_transferred() {
    local mode="$1" src="$2" dst="$3" what="$4" code

    code=$(http_tpc "${mode}" "${src}" "${dst}")

    if [[ "${code}" != "202" ]]; then
        report_copy
        error "${what}: expected 202, got ${code}"
    fi
    # 202 is answered before anything moves; the outcome is in the body.
    if ! grep -q "^success" "${TPCBODY}"; then
        report_copy
        error "${what}: the COPY was accepted but did not report success"
    fi
}

assert_tpc_rejected() {
    local mode="$1" src="$2" dst="$3" what="$4" code

    code=$(http_tpc "${mode}" "${src}" "${dst}")

    if [[ "${code}" != "400" ]]; then
        report_copy
        error "${what}: expected 400, got ${code}"
    fi
}

assert_intact() {
    local host="$1" path="$2" reference="$3" what="$4"
    local fetched="${LCLDATADIR}/fetched.dat"

    download "${host}${path}" "${fetched}"
    cmp -s "${reference}" "${fetched}" || error "${what}: ${path} was damaged"
}

REFFILE="${LCLDATADIR}/random.ref"
${OPENSSL} rand -out "${REFFILE}" $((1024 * (RANDOM + 1)))

# Uploaded to the data servers directly: the cases below depend on knowing which
# server holds which replica.
upload "${HOST_SRV1}${SELFFILE}" "${REFFILE}"
upload "${HOST_SRV1}${GOODFILE}" "${REFFILE}"
upload "${HOST_SRV2}${GOODFILE}" "${REFFILE}"
upload "${HOST_SRV2}${SAMEPATHFILE}" "${REFFILE}"

################################################################################
# Transfers that must keep working
################################################################################

assert_tpc_transferred pull "${HOST_SRV2}${GOODFILE}" "${HOST_SRV1}${RMTDATADIR}/tpc_pull_from_srv2.ref" \
    "pull from srv2 to srv1"
assert_tpc_transferred push "${HOST_SRV1}${GOODFILE}" "${HOST_SRV2}${RMTDATADIR}/tpc_push_to_srv2.ref" \
    "push from srv1 to srv2"

# One path on both ends, which is what a path-only check would wrongly reject.
# It uses its own file rather than SELFFILE, which must keep a single replica.
assert_tpc_transferred pull "${HOST_SRV2}${SAMEPATHFILE}" "${HOST_SRV1}${SAMEPATHFILE}" \
    "pull from srv2 to srv1 under one path"

# The remote end is the manager and the paths differ: following its redirect must
# not be enough on its own to reject.
assert_tpc_transferred pull "${HOST_MAN1}${GOODFILE}" "${HOST_SRV1}${RMTDATADIR}/tpc_pull_from_man.ref" \
    "pull at srv1 sourced from the manager"
assert_tpc_transferred push "${HOST_SRV1}${GOODFILE}" "${HOST_MAN1}${RMTDATADIR}/tpc_push_to_man.ref" \
    "push at srv1 destined to the manager"

# Disabled until TPCHandler::OpenWaitStall() is fixed. A COPY sent to the manager
# opens the destination there, and the cms answers the first open of a file it
# has not located with a stall. OpenWaitStall() sleeps for it and then returns
# the stall rather than re-opening, and the caller reports that as a 403.
#
# The COPY itself is redirected before it runs: once onto a file the manager has
# to place, then over the replica that leaves behind. The two take different
# routes through the cms, which selects a server in the first case and locates
# one in the second.
#assert_tpc_transferred pull "${HOST_SRV2}${GOODFILE}" "${HOST_MAN1}${RMTDATADIR}/tpc_pull_at_man.ref" \
#    "pull at the manager to a new file"
#assert_tpc_transferred pull "${HOST_SRV2}${GOODFILE}" "${HOST_MAN1}${RMTDATADIR}/tpc_pull_at_man.ref" \
#    "pull at the manager over an existing file"

################################################################################
# Copies onto themselves
################################################################################

# Both ends name the server the request went to: the Host header settles it.
assert_tpc_rejected pull "${HOST_SRV1}${SELFFILE}" "${HOST_SRV1}${SELFFILE}" \
    "pull naming srv1 on both ends"
assert_tpc_rejected push "${HOST_SRV1}${SELFFILE}" "${HOST_SRV1}${SELFFILE}" \
    "push naming srv1 on both ends"

# The manager is another address and another port, so only its redirect shows
# that both ends are one file.
assert_tpc_rejected pull "${HOST_MAN1}${SELFFILE}" "${HOST_SRV1}${SELFFILE}" \
    "pull at srv1 sourced from the manager"
assert_tpc_rejected push "${HOST_SRV1}${SELFFILE}" "${HOST_MAN1}${SELFFILE}" \
    "push at srv1 destined to the manager"

# Redirected to the server holding the file, which then sees itself on both ends.
assert_tpc_rejected pull "${HOST_MAN1}${SELFFILE}" "${HOST_MAN1}${SELFFILE}" \
    "pull naming the manager on both ends"
assert_tpc_rejected push "${HOST_MAN1}${SELFFILE}" "${HOST_MAN1}${SELFFILE}" \
    "push naming the manager on both ends"
# Disabled for the same reason as the OpenWaitStall above: this one is not settled by the Host header, so it
# reaches the destination open on the manager and stalls there.
#assert_tpc_rejected pull "${HOST_SRV1}${SELFFILE}" "${HOST_MAN1}${SELFFILE}" \
#    "pull at the manager sourced from srv1"

assert_intact "${HOST_SRV1}" "${SELFFILE}" "${REFFILE}" "a rejected COPY"

echo "ALL TESTS PASSED"
exit 0
