#!/usr/bin/env bash

set -Eexo pipefail

# Skip on macOS due to missing 'declare -A' support

# Check for required commands
: "${ADLER32:=$(command -v xrdadler32)}"
: "${CRC32C:=$(command -v xrdcrc32c)}"
: "${XRDCP:=$(command -v xrdcp)}"
: "${XRDFS:=$(command -v xrdfs)}"
: "${OPENSSL:=$(command -v openssl)}"
: "${CURL:=$(command -v curl)}"

# Function to check for required commands
check_commands() {
    local missing=()
    for PROG in "$@"; do
        if [[ ! -x "${PROG}" ]]; then
            missing+=("${PROG}")
        fi
    done

    if [[ "${#missing[@]}" -gt 0 ]]; then
        echo "ERROR: The following required commands are missing: ${missing[*]}" >&2
        exit 1
    fi
}

function error() {
	echo "error: $*" >&2; exit 1;
}

# shellcheck disable=SC2317
function assert() {
	echo "$@"; "$@" || error "command \"$*\" failed";
}

# $1 is expected_value $2 is received value $3 is the error message
function assert_eq() {
  [[ "$1" == "$2" ]] || error "$3: expected $1 but received $2"
}

# shellcheck disable=SC2317
function assert_failure() {
	echo "$@"; "$@" && error "command \"$*\" did not fail";
}

check_commands "${ADLER32}" "${CRC32C}" "${XRDCP}" "${XRDFS}" "${OPENSSL}" "${CURL}"

# Server host mappings
declare -a hosts=(
    "root://localhost:10951"
    "root://localhost:10952"
)

declare -a hosts_http=(
    "https://localhost:10951"
    "https://localhost:10952"
)

declare -a hosts_abbrev=(
    "srv1"
    "srv2"
)

setup_scitokens() {
	if ! ${XRDSCITOKENS_CREATE_TOKEN} "${XRDSCITOKENS_ISSUER_DIR}"/issuer_pub_1.pem "${XRDSCITOKENS_ISSUER_DIR}"/issuer_key_1.pem test_1 \
		"https://localhost:7095/issuer/one" "storage.modify:/ storage.create:/ storage.read:/" 1800 > "${PWD}/generated_tokens/token"; then
		echo "Failed to create token"
		exit 1
	fi
	chmod 0600 "$PWD/generated_tokens/token"
}

# Cleanup function
# shellcheck disable=SC2317
cleanup() {
    ## Cleanup empty files
    src_idx=0
    dst_idx=1
    src=${hosts_abbrev[${src_idx}]}
    dst=${hosts_abbrev[${dst_idx}]}
    rm "${LCLDATADIR}/${src}_empty.dat" || :
    rm "${LCLDATADIR}/${dst}_empty.ref" || :
    ${XRDFS} "${hosts[$src_idx]}" rm "${RMTDATADIR}/${src}_empty.ref" || :
    for mode in "_http_pull" "_http_push" ""; do
        rm "${LCLDATADIR}/${src}_to_${dst}_empty.dat${mode}" || :
        ${XRDFS} "${hosts[$dst_idx]}" rm "${RMTDATADIR}/${src}_to_${dst}_empty.ref${mode}" || :
    done

    # Cleanup local and remote files
    for src_idx in {0..1}; do
        src=${hosts_abbrev[${src_idx}]}
        rm "${LCLDATADIR}/${src}.dat" || :
        rm "${LCLDATADIR}/${src}.ref" || :

        ${XRDFS} "${hosts[$src_idx]}" rm "${RMTDATADIR}/${src}.ref" || :

        for dst_idx in {0..1}; do
           dst=${hosts_abbrev[${dst_idx}]}
           rm "${LCLDATADIR}/${src}_to_${dst}.dat" || :
           rm "${LCLDATADIR}/${src}_to_${dst}.dat_http_pull" || :
           rm "${LCLDATADIR}/${src}_to_${dst}.dat_http_push" || :

           ${XRDFS} "${hosts[$src_idx]}" rm "${RMTDATADIR}/${dst}_to_${src}.ref" || :
           ${XRDFS} "${hosts[$src_idx]}" rm "${RMTDATADIR}/${dst}_to_${src}.ref_http_push" || :
           ${XRDFS} "${hosts[$src_idx]}" rm "${RMTDATADIR}/${dst}_to_${src}.ref_http_pull" || :
        done
       
    ${XRDFS} "${hosts[$src_idx]}" rmdir "${RMTDATADIR}" || :
    done

    rmdir "${LCLDATADIR}" || :
}
trap "cleanup" ERR



# Set up directories
RMTDATADIR="/srvdata/tpc"
LCLDATADIR="${PWD}/localdata/tpc"
mkdir -p "${LCLDATADIR}"
mkdir -p "${PWD}/generated_tokens"
mkdir -p "$XDG_CACHE_HOME/scitokens" && rm -rf "$XDG_CACHE_HOME/scitokens"/*

# Set up scitokens
setup_scitokens
export BEARER_TOKEN_FILE="$PWD/generated_tokens/token"
BEARER_TOKEN=$(cat "$BEARER_TOKEN_FILE")
export BEARER_TOKEN

generate_file() {
    local local_file=$1
    ${OPENSSL} rand -out "${local_file}" $((1024 * (RANDOM + 1)))
}

generate_empty_file() {
    local local_file=$1
    touch "${local_file}"
}

upload_file() {
    local local_file=$1
    local remote_file=$2
    local protocol=$3
    local http_code

    if [[ -z "${protocol}" || "${protocol}" == "root" ]]; then
        ${XRDCP} "${local_file}" "${remote_file}"
    elif [[ "${protocol}" == "http" ]]; then
        http_code=$(exec 3>&1; ${CURL} -X PUT -L -s -v -o /dev/null -w "%{http_code}" \
            -H "Authorization: Bearer ${BEARER_TOKEN}" \
            -H "Transfer-Encoding: chunked" \
            --cacert "${BINARY_DIR}/tests/issuer/tlsca.pem" \
            --data-binary "@${local_file}" "${remote_file}" \
            2>&1 1>&3 | cat >&2)

        echo "$http_code"
    else
        echo "ERROR: Unsupported protocol: $protocol" >&2
        return 1
    fi
}

perform_tpc() {
    local src_idx=$1
    local dst_idx=$2
    local file_suffix=$3

    if [[ -z "${file_suffix}" ]]; then
        file_suffix=""
    fi

    local src_file="${hosts[$src_idx]}/${RMTDATADIR}/${hosts_abbrev[$src_idx]}${file_suffix}.ref"
    local dst_file="${hosts[$dst_idx]}/${RMTDATADIR}/${hosts_abbrev[$src_idx]}_to_${hosts_abbrev[$dst_idx]}${file_suffix}.ref"

    ${XRDCP} "${src_file}" "${dst_file}"
}

perform_http_tpc() {
    local src_idx=$1
    local dst_idx=$2
    local src=${hosts_abbrev[$src_idx]}
    local dst=${hosts_abbrev[$dst_idx]}
    local mode=$3
    local token_src=$4
    local token_dst=$5
    local file_suffix=$6

    if [[ -z "${file_suffix}" ]]; then
        file_suffix=""
    fi

    local src_file_http="${hosts_http[$src_idx]}/${RMTDATADIR}/${src}${file_suffix}.ref"
    local dst_file_http="${hosts_http[$dst_idx]}/${RMTDATADIR}/${src}_to_${dst}${file_suffix}.ref_http"
    local http_code

    if [[ "$mode" == "push" ]]; then
        dst_file_http="${dst_file_http}_push"
        http_code=$(${CURL} -X COPY -L -s -o >(cat >&2) -w "%{http_code}" \
            -H "Destination: ${dst_file_http}" \
            -H "Authorization: Bearer ${token_dst}" \
            -H "TransferHeaderAuthorization: Bearer ${token_src}" \
            --cacert "${BINARY_DIR}/tests/issuer/tlsca.pem" \
            "${src_file_http}")
    elif [[ "$mode" == "pull" ]]; then
        dst_file_http="${dst_file_http}_pull"
        http_code=$(${CURL} -X COPY -L -s -o >(cat >&2) -w "%{http_code}" \
            -H "Source: ${src_file_http}" \
            -H "Authorization: Bearer ${token_src}" \
            -H "TransferHeaderAuthorization: Bearer ${token_dst}" \
            --cacert "${BINARY_DIR}/tests/issuer/tlsca.pem" \
            "${dst_file_http}")
    else
        echo "ERROR: Unsupported mode: $mode" >&2
        return 1
    fi

    echo "$http_code"
    return 0
}

download_file() {
    local src=$1
    local dest=$2
    local protocol=$3

    if [[ -z "${protocol}" || "${protocol}" == "root" ]]; then
        ${XRDCP} "${src}" "${dest}"
    elif [[ "${protocol}" == "http" ]]; then
        ${CURL} -X GET -L -s -v -o "${dest}" \
            -H "Authorization: Bearer ${BEARER_TOKEN}" \
            -H "Transfer-Encoding: chunked" \
            --cacert "${BINARY_DIR}/tests/issuer/tlsca.pem" \
            "${src}" 2>&1 >&2
    else
        echo "ERROR: Unsupported protocol: $protocol" >&2
        return 1
    fi
}

verify_checksum() {
    local checksum_type=$1
    local ref_file=$2
    local new_file=$3
    local remote_host=$4
    local remote_path=$5

    local TOOL QUERY_TYPE REF_CHECKSUM NEW_CHECKSUM SRV_CHECKSUM

    if [[ "$checksum_type" == "crc32c" ]]; then
        TOOL="${CRC32C}"
        QUERY_TYPE="crc32c"
    elif [[ "$checksum_type" == "adler32" ]]; then
        TOOL="${ADLER32}"
        QUERY_TYPE="adler32"
    else
        echo "ERROR: Unsupported checksum type: $checksum_type"
        exit 1
    fi

    REF_CHECKSUM=$("${TOOL}" < "${ref_file}" | cut -d' ' -f1)
    NEW_CHECKSUM=$("${TOOL}" < "${new_file}" | cut -d' ' -f1)
    SRV_CHECKSUM=$("${XRDFS}" "${remote_host}" query checksum "${remote_path}?cks.type=${QUERY_TYPE}" | cut -d' ' -f2)

    if [[ "${NEW_CHECKSUM}" != "${REF_CHECKSUM}" || "${SRV_CHECKSUM}" != "${REF_CHECKSUM}" ]]; then
        echo "ERROR: ${checksum_type^^} mismatch for ${new_file}, reference: ${REF_CHECKSUM}, server: ${SRV_CHECKSUM}, downloaded: ${NEW_CHECKSUM}"
        exit 1
    fi
}


# Generate, upload, download, and verify checksums for each host
for host_idx in {0..1}; do
    host=${hosts_abbrev[$host_idx]}
    generate_file "${LCLDATADIR}/${host}.ref"
done
 
for host_idx in {0..1}; do
    host=${hosts_abbrev[$host_idx]}
    local_file="${LCLDATADIR}/${host}.ref"
    remote_file="${hosts[$host_idx]}/${RMTDATADIR}/${host}.ref"
    upload_file "${local_file}" "${remote_file}"
done

for host_idx in {0..1}; do
    host=${hosts_abbrev[$host_idx]}
    local_file="${LCLDATADIR}/${host}.dat"
    remote_file="${hosts[$host_idx]}/${RMTDATADIR}/${host}.ref"
    download_file "${remote_file}" "${local_file}"
done

for host_idx in {0..1}; do
    host=${hosts_abbrev[$host_idx]}
    verify_checksum "adler32" "${LCLDATADIR}/${host}.ref" "${LCLDATADIR}/${host}.dat" "${hosts[$host_idx]}" "${RMTDATADIR}/${host}.ref"
    verify_checksum "crc32c" "${LCLDATADIR}/${host}.ref" "${LCLDATADIR}/${host}.dat" "${hosts[$host_idx]}" "${RMTDATADIR}/${host}.ref"
done

# Perform TPC copies between hosts
for src_idx in {0..1}; do
    for dst_idx in {0..1}; do
       perform_tpc "${src_idx}" "${dst_idx}"
       assert_eq "201" "$(perform_http_tpc "$src_idx" "$dst_idx" "pull" "$BEARER_TOKEN" "$BEARER_TOKEN")" "HTTP TPC pull failed"
       assert_eq "201" "$(perform_http_tpc "$src_idx" "$dst_idx" "push" "$BEARER_TOKEN" "$BEARER_TOKEN")" "HTTP TPC push failed"
    done
done

# Download TPC Copies
for src_idx in {0..1}; do
    for dst_idx in {0..1}; do
        src="${hosts_abbrev[$src_idx]}"
        dst="${hosts_abbrev[$dst_idx]}"
        remote_file="${hosts[$dst_idx]}/${RMTDATADIR}/${src}_to_${dst}.ref"
        local_file="${LCLDATADIR}/${src}_to_${dst}.dat"
        download_file "${remote_file}" "${local_file}"

        remote_file_http="${hosts[$dst_idx]}/${RMTDATADIR}/${src}_to_${dst}.ref_http"
        local_file_http="${LCLDATADIR}/${src}_to_${dst}.dat_http"
        # Download files transferred via the pull mode
        download_file "${remote_file_http}_pull" "${local_file_http}_pull"
        # Download files transferred via the push mode
        download_file "${remote_file_http}_push" "${local_file_http}_push"
    done
done

# Verify TPC copies
for src_idx in {0..1}; do
    for dst_idx in {0..1}; do
        src="${hosts_abbrev[$src_idx]}"
        dst="${hosts_abbrev[$dst_idx]}"
        ref_file="${LCLDATADIR}/${src}.ref"
        new_file="${LCLDATADIR}/${src}_to_${dst}.dat"
        remote_file="${hosts[$dst]}/${RMTDATADIR}/${src}_to_${dst}.ref"

        verify_checksum "crc32c" "${ref_file}" "${new_file}" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}.ref"
        verify_checksum "adler32" "${ref_file}" "${new_file}" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}.ref"

        ref_file_http="${LCLDATADIR}/${src}.ref"
        new_file_http="${LCLDATADIR}/${src}_to_${dst}.dat_http"
        remote_file_http="${hosts[$dst_idx]}/${RMTDATADIR}/${src}_to_${dst}.ref_http"

        # Verify checksums for files transferred via the pull mode
        verify_checksum "crc32c" "${ref_file_http}" "${new_file_http}_pull" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}.ref_http_pull"
        verify_checksum "adler32" "${ref_file_http}" "${new_file_http}_pull" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}.ref_http_pull"

        # Verify checksums for files transferred via the push mode
        verify_checksum "crc32c" "${ref_file_http}" "${new_file_http}_push" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}.ref_http_push"
        verify_checksum "adler32" "${ref_file_http}" "${new_file_http}_push" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}.ref_http_push"
    done
done

## Empty files test
src_idx=0
dst_idx=1
src="${hosts_abbrev[$src_idx]}"
dst="${hosts_abbrev[$dst_idx]}"
generate_empty_file "${LCLDATADIR}/${src}_empty.ref"
assert_eq "201" "$(upload_file "${LCLDATADIR}/${src}_empty.ref" "${hosts_http[$src_idx]}/${RMTDATADIR}/${src}_empty.ref" "http")" "HTTP Upload failed"
download_file "${hosts_http[$src_idx]}/${RMTDATADIR}/${src}_empty.ref" "${LCLDATADIR}/${src}_empty.dat" "http"

verify_checksum "crc32c" "${LCLDATADIR}/${src}_empty.ref" "${LCLDATADIR}/${src}_empty.dat" "${hosts[$src_idx]}" "${RMTDATADIR}/${src}_empty.ref"
verify_checksum "adler32" "${LCLDATADIR}/${src}_empty.ref" "${LCLDATADIR}/${src}_empty.dat" "${hosts[$src_idx]}" "${RMTDATADIR}/${src}_empty.ref"

perform_tpc "${src_idx}" "${dst_idx}" "_empty"
assert_eq "201" "$(perform_http_tpc "$src_idx" "$dst_idx" "pull" "$BEARER_TOKEN" "$BEARER_TOKEN" "_empty")" "HTTP TPC pull failed"
assert_eq "201" "$(perform_http_tpc "$src_idx" "$dst_idx" "push" "$BEARER_TOKEN" "$BEARER_TOKEN" "_empty")" "HTTP TPC push failed"

remote_file="${hosts_http[$dst_idx]}/${RMTDATADIR}/${src}_to_${dst}_empty.ref"
local_file="${LCLDATADIR}/${src}_to_${dst}_empty.dat"
download_file "${remote_file}" "${local_file}" "http"
verify_checksum "crc32c" "${LCLDATADIR}/${src}_empty.ref" "${LCLDATADIR}/${src}_to_${dst}_empty.dat" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}_empty.ref"
verify_checksum "adler32" "${LCLDATADIR}/${src}_empty.ref" "${LCLDATADIR}/${src}_to_${dst}_empty.dat" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}_empty.ref"

remote_file_http="${hosts_http[$dst_idx]}/${RMTDATADIR}/${src}_to_${dst}_empty.ref_http"
local_file_http="${LCLDATADIR}/${src}_to_${dst}_empty.dat_http"
download_file "${remote_file_http}_pull" "${local_file_http}_pull" "http"
verify_checksum "crc32c" "${LCLDATADIR}/${src}_empty.ref" "${LCLDATADIR}/${src}_to_${dst}_empty.dat_http_pull" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}_empty.ref_http_pull"
verify_checksum "adler32" "${LCLDATADIR}/${src}_empty.ref" "${LCLDATADIR}/${src}_to_${dst}_empty.dat_http_pull" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}_empty.ref_http_pull"

download_file "${remote_file_http}_push" "${local_file_http}_push" "http"
verify_checksum "crc32c" "${LCLDATADIR}/${src}_empty.ref" "${LCLDATADIR}/${src}_to_${dst}_empty.dat_http_push" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}_empty.ref_http_push"
verify_checksum "adler32" "${LCLDATADIR}/${src}_empty.ref" "${LCLDATADIR}/${src}_to_${dst}_empty.dat_http_push" "${hosts[$dst_idx]}" "${RMTDATADIR}/${src}_to_${dst}_empty.ref_http_push"


cleanup

echo "ALL TESTS PASSED"
exit 0
