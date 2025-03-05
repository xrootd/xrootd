#!/usr/bin/env bash

set -exo pipefail

# Skip on macOS due to missing 'declare -A' support
if [[ $(uname) == "Darwin" ]]; then
       exit 0
fi

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

check_commands "${ADLER32}" "${CRC32C}" "${XRDCP}" "${XRDFS}" "${OPENSSL}" "${CURL}"

# Server host mappings
declare -A hosts=(
    [srv1]="root://localhost:10951"
    [srv2]="root://localhost:10952"
)

declare -A hosts_http=(
    [srv1]="https://localhost:10951"
    [srv2]="https://localhost:10952"
)

cleanup_files() {
    # Cleanup local and remote files
    for src in "${!hosts[@]}"; do
    rm "${LCLDATADIR}/${src}.dat"
    rm "${LCLDATADIR}/${src}.ref"

    ${XRDFS} "${hosts[$src]}" rm "${RMTDATADIR}/${src}.ref"

    for dst in "${!hosts[@]}"; do
           rm "${LCLDATADIR}/${src}_to_${dst}.dat"
           rm "${LCLDATADIR}/${src}_to_${dst}.dat_http_pull"
           rm "${LCLDATADIR}/${src}_to_${dst}.dat_http_push"

           ${XRDFS} "${hosts[$src]}" rm "${RMTDATADIR}/${dst}_to_${src}.ref"
           ${XRDFS} "${hosts[$src]}" rm "${RMTDATADIR}/${dst}_to_${src}.ref_http_push"
           ${XRDFS} "${hosts[$src]}" rm "${RMTDATADIR}/${dst}_to_${src}.ref_http_pull"
    done
       
    ${XRDFS} "${hosts[$src]}" rmdir "${RMTDATADIR}"
    done

    rmdir "${LCLDATADIR}"
}

# Cleanup function
# shellcheck disable=SC2317
cleanup() {
    echo "Error occurred. Cleaning up..."
    set +e
    cleanup_files
    exit 1
}
trap "cleanup" ERR

# Set up directories
RMTDATADIR="/srvdata/tpc"
LCLDATADIR="${PWD}/localdata/tpc"
mkdir -p "${LCLDATADIR}"


## TODO: Cleanup for testing purposes
set +e
cleanup_files > /dev/null 2>&1
set -e
mkdir -p "${LCLDATADIR}"
## Please remove the above block after testing


generate_file() {
    local local_file=$1
    ${OPENSSL} rand -out "${local_file}" $((1024 * (RANDOM + 1)))
}

upload_file() {
    local local_file=$1
    local remote_file=$2
    ${XRDCP} "${local_file}" "${remote_file}"
}

perform_tpc() {
    local src=$1
    local dst=$2
    local src_file="${hosts[$src]}/${RMTDATADIR}/${src}.ref"
    local dst_file="${hosts[$dst]}/${RMTDATADIR}/${src}_to_${dst}.ref"
    ${XRDCP} "${src_file}" "${dst_file}"
}

perform_http_tpc() {
    local src=$1
    local dst=$2
    local mode=$3

    local src_file_http="${hosts_http[$src]}/${RMTDATADIR}/${src}.ref"
    local dst_file_http="${hosts_http[$dst]}/${RMTDATADIR}/${src}_to_${dst}.ref_http"

    if [[ "$mode" == "push" ]]; then
        dst_file_http="${dst_file_http}_push"
        ${CURL} -X COPY -L --verbose \
        -H "Destination: ${dst_file_http}" \
        --cacert "${BINARY_DIR}"/tests/issuer/tlsca.pem \
        "${src_file_http}"
    elif [[ "$mode" == "pull" ]]; then
        dst_file_http="${dst_file_http}_pull"
        ${CURL} -X COPY -L --verbose \
        -H "Source: ${src_file_http}" \
        --cacert "${BINARY_DIR}"/tests/issuer/tlsca.pem \
        "${dst_file_http}"
    else
        echo "ERROR: Unsupported mode: $mode"
        exit 1
    fi
}

download_file() {
    local src=$1
    local dest=$2
    ${XRDCP} "${src}" "${dest}"
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
for host in "${!hosts[@]}"; do
    generate_file "${LCLDATADIR}/${host}.ref"
done
 
for host in "${!hosts[@]}"; do
    local_file="${LCLDATADIR}/${host}.ref"
    remote_file="${hosts[$host]}/${RMTDATADIR}/${host}.ref"
    upload_file "${local_file}" "${remote_file}"
done

for host in "${!hosts[@]}"; do
    local_file="${LCLDATADIR}/${host}.dat"
    remote_file="${hosts[$host]}/${RMTDATADIR}/${host}.ref"
    download_file "${remote_file}" "${local_file}"
done

for host in "${!hosts[@]}"; do
    verify_checksum "adler32" "${LCLDATADIR}/${host}.ref" "${LCLDATADIR}/${host}.dat" "${hosts[$host]}" "${RMTDATADIR}/${host}.ref"
    verify_checksum "crc32c" "${LCLDATADIR}/${host}.ref" "${LCLDATADIR}/${host}.dat" "${hosts[$host]}" "${RMTDATADIR}/${host}.ref"
done

# Perform TPC copies between hosts
for src in "${!hosts[@]}"; do
    for dst in "${!hosts[@]}"; do     
       perform_tpc "${src}" "${dst}"
       perform_http_tpc "${src}" "${dst}" "pull"
       perform_http_tpc "${src}" "${dst}" "push"
    done
done

# Download TPC Copies
for src in "${!hosts[@]}"; do
    for dst in "${!hosts[@]}"; do
        remote_file="${hosts[$dst]}/${RMTDATADIR}/${src}_to_${dst}.ref"
        local_file="${LCLDATADIR}/${src}_to_${dst}.dat"
        download_file "${remote_file}" "${local_file}"

        remote_file_http="${hosts[$dst]}/${RMTDATADIR}/${src}_to_${dst}.ref_http"
        local_file_http="${LCLDATADIR}/${src}_to_${dst}.dat_http"
        # Download files transferred via the pull mode
        download_file "${remote_file_http}_pull" "${local_file_http}_pull"
        # Download files transferred via the push mode
        download_file "${remote_file_http}_push" "${local_file_http}_push"
    done
done

# Verify TPC copies
for src in "${!hosts[@]}"; do
    for dst in "${!hosts[@]}"; do
        ref_file="${LCLDATADIR}/${src}.ref"
        new_file="${LCLDATADIR}/${src}_to_${dst}.dat"
        remote_file="${hosts[$dst]}/${RMTDATADIR}/${src}_to_${dst}.ref"

        verify_checksum "crc32c" "${ref_file}" "${new_file}" "${hosts[$dst]}" "${RMTDATADIR}/${src}_to_${dst}.ref"
        verify_checksum "adler32" "${ref_file}" "${new_file}" "${hosts[$dst]}" "${RMTDATADIR}/${src}_to_${dst}.ref"

        ref_file_http="${LCLDATADIR}/${src}.ref"
        new_file_http="${LCLDATADIR}/${src}_to_${dst}.dat_http"
        remote_file_http="${hosts[$dst]}/${RMTDATADIR}/${src}_to_${dst}.ref_http"

        # Verify checksums for files transferred via the pull mode
        verify_checksum "crc32c" "${ref_file_http}" "${new_file_http}_pull" "${hosts[$dst]}" "${RMTDATADIR}/${src}_to_${dst}.ref_http_pull"
        verify_checksum "adler32" "${ref_file_http}" "${new_file_http}_pull" "${hosts[$dst]}" "${RMTDATADIR}/${src}_to_${dst}.ref_http_pull"

        # Verify checksums for files transferred via the push mode
        verify_checksum "crc32c" "${ref_file_http}" "${new_file_http}_push" "${hosts[$dst]}" "${RMTDATADIR}/${src}_to_${dst}.ref_http_push"
        verify_checksum "adler32" "${ref_file_http}" "${new_file_http}_push" "${hosts[$dst]}" "${RMTDATADIR}/${src}_to_${dst}.ref_http_push"
    done
done

cleanup_files

echo "ALL TESTS PASSED"
exit 0