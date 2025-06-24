#!/bin/bash
set -x

NUM_FILES=10

# Clean up any old files
for i in $(seq 1 $NUM_FILES); do
    rm -f "${LCLDATADIR}/largefile.ref.${i}" \
          "${LCLDATADIR}/largefile.dat.${i}" \
          "${PWD}/data/srv1/srvdata/tpc/largefile.ref.${i}" \
          "${PWD}/data/srv2/srvdata/tpc/largefile.ref.${i}"
done

# Generate, upload, and test parallel COPY with random cancellation
for i in $(seq 1 $NUM_FILES); do
    local_large_file="${LCLDATADIR}/largefile.ref.${i}"
    remote_large_file="https://localhost:10951/${RMTDATADIR}/largefile.ref.${i}"
    downloaded_large_file="${LCLDATADIR}/largefile.dat.${i}"

    generate_file "${local_large_file}" 500000000
    upload_file "${local_large_file}" "${remote_large_file}" http
    download_file "${remote_large_file}" "${downloaded_large_file}" http

done

wait

for i in $(seq 1 $NUM_FILES); do
    scitag_flow=$((65 + (RANDOM % 4)))
    local_large_file="${LCLDATADIR}/largefile.ref.${i}"
    remote_large_file="https://localhost:10951/${RMTDATADIR}/largefile.ref.${i}"
    downloaded_large_file="${LCLDATADIR}/largefile.dat.${i}"

    remote_large_file_srvs=(
        "https://localhost:10951/${RMTDATADIR}/largefile.ref1.${i}"
        "https://localhost:10952/${RMTDATADIR}/largefile.ref2.${i}"
    )	

    for remote_large_file_srv in "${remote_large_file_srvs[@]}"; do
    # Randomly cancel some requests
        if (( 3 > ( RANDOM % 5 ))); then
            ${CURL} -X COPY -L -s -v \
                -H "Destination: ${remote_large_file_srv}" \
                -H "Authorization: Bearer ${BEARER_TOKEN}" \
                -H "TransferHeaderAuthorization: Bearer ${BEARER_TOKEN}" \
                -H "Scitag: ${scitag_flow}" \
                --cacert "${BINARY_DIR}/tests/issuer/tlsca.pem" \
                --max-time 1 \
                "${remote_large_file}" &
        else
            # No max-time (normal)
            ${CURL} -X COPY -L -s -v \
                -H "Destination: ${remote_large_file_srv}" \
                -H "Authorization: Bearer ${BEARER_TOKEN}" \
                -H "TransferHeaderAuthorization: Bearer ${BEARER_TOKEN}" \
                -H "Scitag: ${scitag_flow}" \
                --cacert "${BINARY_DIR}/tests/issuer/tlsca.pem" \
                "${remote_large_file}" &
        fi
    done
done

# Wait for all background curl jobs
wait

set +x
