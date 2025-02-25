#!/usr/bin/env bash

# Skip on macOS due to missing 'declare -A' support
if [[ $(uname) == "Darwin" ]]; then
       exit 0
fi

# Check for required commands
: ${ADLER32:=$(command -v xrdadler32)}
: ${CRC32C:=$(command -v xrdcrc32c)}
: ${XRDCP:=$(command -v xrdcp)}
: ${XRDFS:=$(command -v xrdfs)}
: ${OPENSSL:=$(command -v openssl)}
: ${CURL:=$(command -v curl)}

# Server host mappings
: ${HOST_SRV1:=root://localhost:10943}
: ${HOST_SRV2:=root://localhost:10944}
: ${HOST_SRV3:=root://localhost:10945}
: ${HOST_SRV4:=root://localhost:10946}


# Checking command existence
for PROG in ${ADLER32} ${CRC32C} ${XRDCP} ${XRDFS} ${OPENSSL} ${CURL}; do
       if [[ ! -x "${PROG}" ]]; then
               echo 1>&2 "$(basename $0): error: '${PROG}': command not found"
               exit 1
       fi
done

# Set up directories
RMTDATADIR="/srvdata"
LCLDATADIR="${PWD}/localdata"
LCLDATADIR_TPC="${LCLDATADIR}/tpc"
mkdir -p ${LCLDATADIR_TPC}

# Define server list
declare -A hosts
hosts["srv1"]="${HOST_SRV1}"
hosts["srv2"]="${HOST_SRV2}"
hosts["srv3"]="${HOST_SRV3}"
hosts["srv4"]="${HOST_SRV4}"


# Cleanup function
cleanup() {
       echo "Error occurred. Cleaning up..."
       for host in "${!hosts[@]}"; do
              rm -rf ${LCLDATADIR}/tpc/${host}.dat
              rm -rf ${LCLDATADIR}/tpc/${host}.ref
       done
}
trap "cleanup; exit 1" ABRT

# Generate random files
for host in "${!hosts[@]}"; do
       ${OPENSSL} rand -out "${LCLDATADIR}/tpc/${host}.ref" $((1024 * ($RANDOM + 1)))
done

# Upload local files to servers
for host in "${!hosts[@]}"; do
       ${XRDCP} ${LCLDATADIR}/tpc/${host}.ref ${hosts[$host]}/${RMTDATADIR}/tpc/${host}.ref &
done
wait  # Ensure uploads complete

# Perform Third-Party Copies (TPC) between servers
for src in "${!hosts[@]}"; do
    for dst in "${!hosts[@]}"; do
        echo "Performing TPC from ${src} -> ${dst}"
        ${XRDCP} ${hosts[$src]}/${RMTDATADIR}/tpc/${src}.ref ${hosts[$dst]}/${RMTDATADIR}/tpc/${src}_to_${dst}.ref &
    done
done
wait  # Ensure TPCs complete

# List files on each server
for host in "${!hosts[@]}"; do
       echo "Files on ${host}:"
       ${XRDFS} ${hosts[$host]} ls -l ${RMTDATADIR}/tpc
done

# Download and verify original files
for host in "${!hosts[@]}"; do
       ${XRDCP} ${hosts[$host]}/${RMTDATADIR}/tpc/${host}.ref ${LCLDATADIR}/tpc/${host}.dat
done

# Verify Checksums (CRC32C & Adler32)
for host in "${!hosts[@]}"; do
       REF32C=$(${CRC32C} < ${LCLDATADIR}/tpc/${host}.ref | cut -d' ' -f1)
       NEW32C=$(${CRC32C} < ${LCLDATADIR}/tpc/${host}.dat | cut -d' ' -f1)
       SRV32C=$(${XRDFS} ${hosts[$host]} query checksum ${RMTDATADIR}/tpc/${host}.ref?cks.type=crc32c | cut -d' ' -f2)

       if [[ "${NEW32C}" != "${REF32C}" || "${SRV32C}" != "${REF32C}" ]]; then
              echo "${host}:  CRC32C mismatch!"
              echo "${host}:  crc32c: reference: ${REF32C}, server: ${SRV32C}, downloaded: ${NEW32C}"
              echo 1>&2 "$(basename $0): error: crc32 checksum check failed for file: ${host}.dat"
              exit 1
       fi

       REFA32=$(${ADLER32} < ${LCLDATADIR}/tpc/${host}.ref | cut -d' ' -f1)
       NEWA32=$(${ADLER32} < ${LCLDATADIR}/tpc/${host}.dat | cut -d' ' -f1)
       SRVA32=$(${XRDFS} ${hosts[$host]} query checksum ${RMTDATADIR}/tpc/${host}.ref?cks.type=adler32 | cut -d' ' -f2)

       if [[ "${NEWA32}" != "${REFA32}" || "${SRVA32}" != "${REFA32}" ]]; then
               echo "${host}:  Adler32 mismatch!"
               echo "${host}:  adler32: reference: ${REFA32}, server: ${SRVA32}, downloaded: ${NEWA32}"
               echo 1>&2 "$(basename $0): error: adler32 checksum check failed for file: ${host}.dat"
               exit 1
       fi
done

# Download and verify TPC files
for src in "${!hosts[@]}"; do
    for dst in "${!hosts[@]}"; do
       ${XRDCP} ${hosts[$dst]}/${RMTDATADIR}/tpc/${src}_to_${dst}.ref ${LCLDATADIR}/tpc/${src}_to_${dst}.dat
    done
done

# Verify TPC copies
for src in "${!hosts[@]}"; do
    for dst in "${!hosts[@]}"; do
        echo "Verifying TPC from ${src} -> ${dst}"
        REF32C=$(${CRC32C} < ${LCLDATADIR}/tpc/${src}.ref | cut -d' ' -f1)
        NEW32C=$(${CRC32C} < ${LCLDATADIR}/tpc/${src}_to_${dst}.dat | cut -d' ' -f1)
        SRV32C=$(${XRDFS} ${hosts[$dst]} query checksum ${RMTDATADIR}/tpc/${src}_to_${dst}.ref?cks.type=crc32c | cut -d' ' -f2)
        if [[ "${SRV32C}" != "${REF32C}" || "${NEW32C}" != "${REF32C}" ]]; then
            echo "ERROR: CRC32C mismatch for ${src}_to_${dst}"
            echo "${src}_to_${dst}:  crc32c: reference: ${REF32C}, server: ${SRV32C}, downloaded: ${NEW32C}"
            exit 1
        fi
        REFA32=$(${ADLER32} < ${LCLDATADIR}/tpc/${src}.ref | cut -d' ' -f1)
        NEWA32=$(${ADLER32} < ${LCLDATADIR}/tpc/${src}_to_${dst}.dat | cut -d' ' -f1)
        SRVA32=$(${XRDFS} ${hosts[$dst]} query checksum ${RMTDATADIR}/tpc/${src}_to_${dst}.ref?cks.type=adler32 | cut -d' ' -f2)
        if [[ "${SRVA32}" != "${REFA32}" || "${NEWA32}" != "${REFA32}" ]]; then
            echo "ERROR: Adler32 mismatch for ${src}_to_${dst}"
            echo "${src}_to_${dst}:  adler32: reference: ${REFA32}, server: ${SRVA32}, downloaded: ${NEWA32}"
            exit 1
        fi
    done
done

# Cleanup remote and local files
for host in "${!hosts[@]}"; do
       ${XRDFS} ${hosts[$host]} rm ${RMTDATADIR}/tpc/${host}.ref &
       rm ${LCLDATADIR}/tpc/${host}.dat &
       for dst in "${!hosts[@]}"; do
                ${XRDFS} ${hosts[$dst]} rm ${RMTDATADIR}/tpc/${host}_to_${dst}.ref &
                rm ${LCLDATADIR}/tpc/${host}_to_${dst}.dat &
       done
done
wait


echo "ALL TESTS PASSED"
exit 0