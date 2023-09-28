#!/bin/bash
#
# This is a sample script meant to be used as the program argument of an
# ofs.tpc configuration directive for the xrootd server, for example:
#
# ofs.tpc fcreds ?gsi =X509_USER_PROXY pgm /usr/share/xrootd/utils/xrdcp-tpc.sh
#

OPTS=("${@:1:$#-2}")

shift $(($# - 2))

SRC=$1
DST=$2

# If a TPC gateway is used, the destination has to be adjusted to prepend
# root://${XRDXROOTD_ORIGIN} because otherwise the gateway will try to copy
# data into its own local filesystem. If no gateway is used, this part of
# the script can be removed.

if [[ -n "${XRDXROOTD_ORIGIN}" ]]; then
	DST="root://${XRDXROOTD_ORIGIN}/${DST}"
fi

# CGI info from URLs can leak credentials in the system log, so remove them

SAFE_SRC=${SRC/\?*}
SAFE_DST=${DST/\?*}

logger -p info -t xrdcp-tpc "start transfer: ${SAFE_SRC} => ${SAFE_DST}"

# It is important to ensure that the --server option is passed to the client,
# so that it loads and uses the necessary variables from the environment which
# are set by the server. This script adds logging to demonstrate how to leave
# a trace of each transfer that can be checked with journalctl. It is also
# important to exit with a non-zero status when a transfer fails (that is,
# ensure that the exit status is that of the xrdcp command, not the logger
# command which runs after it).

xrdcp --server "${OPTS[@]}" "${SRC}" "${DST}"

STATUS=$?

if [[ ${STATUS} -eq 0 ]]; then
	logger -p info -t xrdcp-tpc "transfer: ${SAFE_SRC} => ${SAFE_DST} completed successfully"
else
	logger -p err  -t xrdcp-tpc "transfer: ${SAFE_SRC} => ${SAFE_DST} FAILED [exit code: ${STATUS}]"
fi

exit ${STATUS}
