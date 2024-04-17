#!/bin/bash

# This script checks that each installed public and private XRootD header can
# be included individually without errors. The intention is to identify which
# headers may have missing includes, missing forward declarations, or missing
# header dependencies, that is, headers from XRootD which it includes, but were
# not installed by the install target.

# We need to split CXXFLAGS
# shellcheck disable=SC2086

: "${CXX:=c++}"
: "${CXXFLAGS:=-Wall}"
: "${INCLUDE_DIR:=${1:-/usr/include/xrootd}}"

if ! command -v "${CXX}" >/dev/null; then
	exit 1
fi

STATUS=0

HEADERS=$(find "${INCLUDE_DIR}" -type f -name '*.hh')
PUBLIC_HEADERS=$(grep -E -v '(XrdPosix|private)' <<< "${HEADERS}")
PRIVATE_HEADERS=$(grep private <<< "${HEADERS}")

# Check public headers without adding private include directory.
# This ensures public headers do not depend on any private headers.

while IFS=$'\n' read -r HEADER; do
	"${CXX}" -fsyntax-only ${CXXFLAGS} -I"${INCLUDE_DIR}" "${HEADER}" || STATUS=1
done <<< "${PUBLIC_HEADERS}"

# Check private headers

while IFS=$'\n' read -r HEADER; do
	"${CXX}" -fsyntax-only ${CXXFLAGS} -I"${INCLUDE_DIR}"{,/private} "${HEADER}" || STATUS=1
done <<< "${PRIVATE_HEADERS}"

exit $STATUS
