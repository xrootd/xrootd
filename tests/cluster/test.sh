#!/usr/bin/env bash

# macOS, as of now, cannot run this test because of the 'declare -A'
# command that we use later, so we just skip this test (sorry apple users)
if [[ $(uname) == "Darwin" ]]; then
       exit 0
fi

# we probably need all of these still
: ${ADLER32:=$(command -v xrdadler32)}
: ${CRC32C:=$(command -v xrdcrc32c)}
: ${XRDCP:=$(command -v xrdcp)}
: ${XRDFS:=$(command -v xrdfs)}
: ${OPENSSL:=$(command -v openssl)}
: ${HOST_METAMAN:=root://localhost:10940}
: ${HOST_MAN1:=root://localhost:10941}
: ${HOST_MAN2:=root://localhost:10942}
: ${HOST_SRV1:=root://localhost:10943}
: ${HOST_SRV2:=root://localhost:10944}
: ${HOST_SRV3:=root://localhost:10945}
: ${HOST_SRV4:=root://localhost:10946}

# checking for command presence
for PROG in ${ADLER32} ${CRC32C} ${XRDCP} ${XRDFS} ${OPENSSL}; do
       if [[ ! -x "${PROG}" ]]; then
               echo 1>&2 "$(basename $0): error: '${PROG}': command not found"
               exit 1
       fi
done

# This script assumes that ${host} exports an empty / as read/write.
# It also assumes that any authentication required is already setup.

set -xe

echo "xrdcp/fs-test1"
${XRDCP} --version

for host in "${!hosts[@]}"; do
       ${XRDFS} ${hosts[$host]} query config version
done

# query some common server configurations

CONFIG_PARAMS=( version role sitename )

for PARAM in ${CONFIG_PARAMS[@]}; do
       for host in "${!hosts[@]}"; do
              ${XRDFS} ${hosts[$host]} query config ${PARAM}
       done
done

# some extra query commands that don't make any changes
${XRDFS} ${HOST_METAMAN} stat /
${XRDFS} ${HOST_METAMAN} statvfs /
${XRDFS} ${HOST_METAMAN} spaceinfo /

RMTDATADIR="/srvdata"
LCLDATADIR="${PWD}/localdata"  # client folder

mkdir -p ${LCLDATADIR}

# hostname-address pair, so that we can keep track of files more easily
declare -A hosts
hosts["metaman"]="${HOST_METAMAN}"
hosts["man1"]="${HOST_MAN1}"
hosts["man2"]="${HOST_MAN2}"
hosts["srv1"]="${HOST_SRV1}"
hosts["srv2"]="${HOST_SRV2}"
hosts["srv3"]="${HOST_SRV3}"
hosts["srv4"]="${HOST_SRV4}"

cleanup() {
       echo "Error occured. Cleaning up..."
       for host in "${!hosts[@]}"; do
              rm -rf ${LCLDATADIR}/${host}.dat
              rm -rf ${LCLDATADIR}/${host}.ref
       done
}
trap "cleanup; exit 1" ABRT

# create local files with random contents using OpenSSL
echo "Creating files for each instance..."

for host in "${!hosts[@]}"; do
       ${OPENSSL} rand -out "${LCLDATADIR}/${host}.ref" $((1024 * $RANDOM))
done

# upload local files to the servers in parallel
echo "Uploading files..."


for host in "${!hosts[@]}"; do
       ${XRDCP} ${LCLDATADIR}/${host}.ref ${hosts[$host]}/${RMTDATADIR}/${host}.ref
done

# list uploaded files, then download them to check for corruption
echo "Downloading them back..."

for host in "${!hosts[@]}"; do
       ${XRDFS} ${hosts[$host]} ls -l ${RMTDATADIR}
done

echo "Downloading them back... pt2"

for host in "${!hosts[@]}"; do
       ${XRDCP} ${hosts[$host]}/${RMTDATADIR}/${host}.ref ${LCLDATADIR}/${host}.dat
done

# check that all checksums for downloaded files match
echo "Comparing checksum..."

for host in "${!hosts[@]}"; do
       REF32C=$(${CRC32C} < ${LCLDATADIR}/${host}.ref | cut -d' '  -f1)
       NEW32C=$(${CRC32C} < ${LCLDATADIR}/${host}.dat | cut -d' '  -f1)
       SRV32C=$(${XRDFS} ${hosts[$host]} query checksum ${RMTDATADIR}/${host}.ref?cks.type=crc32c | cut -d' ' -f2)

       REFA32=$(${ADLER32} < ${LCLDATADIR}/${host}.ref | cut -d' '  -f1)
       NEWA32=$(${ADLER32} < ${LCLDATADIR}/${host}.dat | cut -d' '  -f1)
       SRVA32=$(${XRDFS} ${hosts[$host]} query checksum ${RMTDATADIR}/${host}.ref?cks.type=adler32 | cut -d' ' -f2)
       echo "${host}:  crc32c: reference: ${REF32C}, server: ${SRV32C}, downloaded: ${REF32C}"
       echo "${host}: adler32: reference: ${NEWA32}, server: ${SRVA32}, downloaded: ${NEWA32}"

       if [[ "${NEWA32}" != "${REFA32}" || "${SRVA32}" != "${REFA32}" ]]; then
               echo 1>&2 "$(basename $0): error: adler32 checksum check failed for file: ${host}.dat"
               exit 1r
       fi
       if [[ "${NEW32C}" != "${REF32C}" || "${SRV32C}" != "${REF32C}" ]]; then
               echo 1>&2 "$(basename $0): error: crc32 checksum check failed for file: ${host}.dat"
               exit 1
       fi
done

echo "All good! Now removing stuff..."

for host in "${!hosts[@]}"; do
       ${XRDFS} ${HOST_METAMAN} rm ${RMTDATADIR}/${host}.ref &
       rm ${LCLDATADIR}/${host}.dat &
done

wait

${XRDFS} ${HOST_METAMAN} rmdir ${RMTDATADIR}

echo "ALL TESTS PASSED"
exit 0
