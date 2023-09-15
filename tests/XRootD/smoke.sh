#!/usr/bin/env bash

set -e

: ${ADLER32:=$(command -v xrdadler32)}
: ${CRC32C:=$(command -v xrdcrc32c)}
: ${XRDCP:=$(command -v xrdcp)}
: ${XRDFS:=$(command -v xrdfs)}
: ${OPENSSL:=$(command -v openssl)}
: ${HOST:=root://localhost:${PORT:-1094}}

for PROG in ${ADLER32} ${CRC32C} ${XRDCP} ${XRDFS} ${OPENSSL}; do
       if [[ ! -x "${PROG}" ]]; then
               echo 1>&2 "$(basename $0): error: '${PROG}': command not found"
               exit 1
       fi
done

# This script assumes that ${HOST} exports an empty / as read/write.
# It also assumes that any authentication required is already setup.

echo Using ${OPENSSL}: $(${OPENSSL} version)
echo Using ${XRDCP}: $(${XRDCP} --version)

${XRDFS} ${HOST} query config version

# query some common server configurations

CONFIG_PARAMS=( version role sitename )

for PARAM in ${CONFIG_PARAMS[@]}; do
       ${XRDFS} ${HOST} query config ${PARAM}
done

# some extra query commands that don't make any changes

${XRDFS} ${HOST} stat /
${XRDFS} ${HOST} statvfs /
${XRDFS} ${HOST} spaceinfo /

# create local temporary directory
TMPDIR=$(mktemp -d /tmp/xrdfs-test-XXXXXX)

# cleanup after ourselves if something fails
trap "rm -rf ${TMPDIR}" EXIT

# create remote temporary directory
# this will get cleaned up by CMake upon fixture tear down
${XRDFS} ${HOST} mkdir -p ${TMPDIR}

# create local files with random contents using OpenSSL

FILES=$(seq -w 1 ${NFILES:-10})

for i in $FILES; do
       ${OPENSSL} rand -out "${TMPDIR}/${i}.ref" $((1024 * $RANDOM))
done

# upload local files to the server in parallel

for i in $FILES; do
       ${XRDCP} ${TMPDIR}/${i}.ref ${HOST}/${TMPDIR}/${i}.ref
done

# list uploaded files, then download them to check for corruption

${XRDFS} ${HOST} ls -l ${TMPDIR}

for i in $FILES; do
       ${XRDCP} ${HOST}/${TMPDIR}/${i}.ref ${TMPDIR}/${i}.dat
done

# check that all checksums for downloaded files match

for i in $FILES; do
       REF32C=$(${CRC32C} < ${TMPDIR}/${i}.ref | cut -d' '  -f1)
       NEW32C=$(${CRC32C} < ${TMPDIR}/${i}.dat | cut -d' '  -f1)
       SRV32C=$(${XRDFS} ${HOST} query checksum ${TMPDIR}/${i}.ref?cks.type=crc32c | cut -d' ' -f2)

       REFA32=$(${ADLER32} < ${TMPDIR}/${i}.ref | cut -d' '  -f1)
       NEWA32=$(${ADLER32} < ${TMPDIR}/${i}.dat | cut -d' '  -f1)
       SRVA32=$(${XRDFS} ${HOST} query checksum ${TMPDIR}/${i}.ref?cks.type=adler32 | cut -d' ' -f2)
       echo "${i}:  crc32c: reference: ${REF32C}, server: ${SRV32C}, downloaded: ${REF32C}"
       echo "${i}: adler32: reference: ${NEWA32}, server: ${SRVA32}, downloaded: ${NEWA32}"

       if [[ "${NEWA32}" != "${REFA32}" || "${SRVA32}" != "${REFA32}" ]]; then
               echo 1>&2 "$(basename $0): error: adler32 checksum check failed for file: ${i}.dat"
               exit 1
       fi
       if [[ "${NEW32C}" != "${REF32C}" || "${SRV32C}" != "${REF32C}" ]]; then
               echo 1>&2 "$(basename $0): error: crc32 checksum check failed for file: ${i}.dat"
               exit 1
       fi
done

for i in $FILES; do
       ${XRDFS} ${HOST} rm ${TMPDIR}/${i}.ref &
done

wait

${XRDFS} ${HOST} rmdir ${TMPDIR}

echo "ALL TESTS PASSED"
exit 0
