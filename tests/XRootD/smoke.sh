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
TMPDIR=$(mktemp -d ${PWD}/xrdfs-test-XXXXXX)

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

    REFA32=$(${ADLER32} < ${TMPDIR}/${i}.ref | cut -d' '  -f1)
    NEWA32=$(${ADLER32} < ${TMPDIR}/${i}.dat | cut -d' '  -f1)

    if setfattr -n user.checksum -v ${REF32C} ${TMPDIR}/${i}.ref; then
        SRV32C=$(${XRDFS} ${HOST} query checksum ${TMPDIR}/${i}.ref?cks.type=crc32c | cut -d' ' -f2)
        SRVA32=$(${XRDFS} ${HOST} query checksum ${TMPDIR}/${i}.ref?cks.type=adler32 | cut -d' ' -f2)
    else
        echo "Extended attributes not supported, using downloaded checksums for server checks"
        SRV32C=${NEW32C} SRVA32=${NEWA32} # use downloaded file checksum if xattr not supported
    fi

    if [[ "${NEWA32}" != "${REFA32}" || "${SRVA32}" != "${REFA32}" ]]; then
        echo 1>&2 "$(basename $0): error: adler32 checksum check failed for file: ${i}.dat"
        echo 1>&2 "${i}: adler32: reference: ${REFA32}, server: ${SRVA32}, downloaded: ${NEWA32}"
        exit 1
    fi

    if [[ "${NEW32C}" != "${REF32C}" || "${SRV32C}" != "${REF32C}" ]]; then
        echo 1>&2 "$(basename $0): error: crc32 checksum check failed for file: ${i}.dat"
        echo 1>&2 "${i}:  crc32c: reference: ${REF32C}, server: ${SRV32C}, downloaded: ${NEW32C}"
        exit 1
    fi
done

for i in $FILES; do
       ${XRDFS} ${HOST} rm ${TMPDIR}/${i}.ref &
done

wait

#
# check return code for xrdfs rm
# create another 6 files, which should be deleted during the test
#
for i in $(seq -w 1 6) ; do
       ${OPENSSL} rand -out "${TMPDIR}/${i}.exists.ref" $((1024 * $RANDOM))
       ${XRDCP} ${TMPDIR}/${i}.exists.ref ${HOST}/${TMPDIR}/${i}.exists.ref
done

# remove 3 existing, should succeed not error
${XRDFS} ${HOST} rm ${TMPDIR}/1.exists.ref ${TMPDIR}/2.exists.ref ${TMPDIR}/3.exists.ref

set +e

# remove 3 not existing, should error
#
${XRDFS} ${HOST} rm ${TMPDIR}/not_exists_1.ref ${TMPDIR}/not_exists_2.ref ${TMPDIR}/not_exists_3.ref
rm_rc=$?
if [ $rm_rc -eq 0 ]; then
  exit 1
fi
#
# remove 2 existing, 1 not existing should error
#
${XRDFS} ${HOST} rm ${TMPDIR}/4.exists.ref ${TMPDIR}/5.exists.ref ${TMPDIR}/not_exists_4.ref
rm_rc=$?
if [ $rm_rc -eq 0 ]; then
  exit 1
fi
#
# remove 1 existing, 2 not existing should error
#
${XRDFS} ${HOST} rm ${TMPDIR}/6.exists.ref ${TMPDIR}/not_exists_5.ref ${TMPDIR}/not_exists_6.ref
rm_rc=$?
if [ $rm_rc -eq 0 ]; then
  exit 1
fi
set -e

${XRDFS} ${HOST} rmdir ${TMPDIR}

echo "ALL TESTS PASSED"
exit 0
