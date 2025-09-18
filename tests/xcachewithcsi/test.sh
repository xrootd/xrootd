#!/usr/bin/env bash

# macOS, as of now, cannot run this test because of the 'declare -A'
# command that we use later, so we just skip this test (sorry apple users)
if [[ $(uname) == "Darwin" ]]; then
       exit 0
fi

# we probably need all of these still
: ${XRDCP:=$(command -v xrdcp)}
: ${XRDFS:=$(command -v xrdfs)}
: ${TIMEOUTCMD:=$(command -v timeout)}
: ${HOST_SRV1:=root://localhost:10981}
: ${HOST_SRV2:=root://localhost:10982}

# checking for command presence
for PROG in ${XRDCP} ${XRDFS} ${TIMEOUTCMD}; do
       if [[ ! -x "${PROG}" ]]; then
               echo 1>&2 "$(basename $0): error: '${PROG}': command not found"
               exit 1
       fi
done

# This script assumes that ${host} exports an empty / as read/write.
# It also assumes that any authentication required is already setup.

set -xe

${XRDCP} --version

# hostname-address pair, so that we can keep track of files more easily
declare -A hosts
hosts["srv1"]="${HOST_SRV1}"
hosts["srv2"]="${HOST_SRV2}"

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

XRD_CONNECTIONRETRY=0 ${TIMEOUTCMD} 10 ${XRDCP} /etc/hosts ${HOST_SRV1}//file1

XRD_CONNECTIONRETRY=0 ${TIMEOUTCMD} 10 ${XRDCP} -f ${HOST_SRV2}//file1 /dev/null

dd if=/dev/zero of=data/srv-pfc/file1 bs=1 count=1 conv=notrunc

set +e
XRD_CONNECTIONRETRY=0 ${TIMEOUTCMD} 10 ${XRDCP} -f ${HOST_SRV2}//file1 /dev/null 2> /tmp/err.txt
ret1=$?
grep -q "Run: \[ERROR\] Server responded with an error: \[3019\]" /tmp/err.txt
ret2=$?
cat /tmp/err.txt
if [ $ret1 -ne 54 -o $ret2 -ne 0 ]; then
  echo "${host}: did not get the expected error on corrupted file"
  exit 1
fi

set -e
rm -f /tmp/err.txt

echo "ALL TESTS PASSED"
exit 0
