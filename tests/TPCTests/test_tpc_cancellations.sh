#!/bin/bash
set -x

NUM_FILES=9
NUM_STREAMS=3

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

    generate_file "${local_large_file}" 200000000 &
    wait
    upload_file "${local_large_file}" "${remote_large_file}" http &
    wait
    download_file "${remote_large_file}" "${downloaded_large_file}" http &
    wait

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
        if (( NUM_FILES/3 > ( RANDOM % NUM_FILES))); then
            ${CURL} -X COPY -L -s -v \
                -H "Destination: ${remote_large_file_srv}" \
                -H "Authorization: Bearer ${BEARER_TOKEN}" \
                -H "TransferHeaderAuthorization: Bearer ${BEARER_TOKEN}" \
                -H "Scitag: ${scitag_flow}" \
                --cacert "${BINARY_DIR}/tests/issuer/tlsca.pem" \
                --max-time 1 \
                "${remote_large_file}" &
        elif (( NUM_FILES*2/3 > (RANDOM % NUM_FILES) )); then
            # Multstream is only implemented in pull mode
            # No max-time (normal) 
            ${CURL} -X COPY -L -s -v \
                -H "Source: ${remote_large_file}" \
                -H "Authorization: Bearer ${BEARER_TOKEN}" \
                -H "TransferHeaderAuthorization: Bearer ${BEARER_TOKEN}" \
                -H "Scitag: ${scitag_flow}" \
                -H "X-Number-Of-Streams: $NUM_STREAMS" \
                --cacert "${BINARY_DIR}/tests/issuer/tlsca.pem" \
                "${remote_large_file_srv}" &
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
