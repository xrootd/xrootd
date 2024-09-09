#!/usr/bin/env bash

set -e

function error() {
	echo "error: $*" >&2; exit 1;
}

function assert() {
	echo "$@"; "$@" || error "command \"$*\" failed";
}

function assert_failure() {
	echo "$@"; "$@" && error "command \"$*\" did not fail";
}

function require_commands() {
	for PROG in "$@"; do
	       if [[ ! -x "$(command -v "${PROG}")" ]]; then
		       error "'${PROG}': command not found"
	       fi
	done
}

[[ -n "$1" ]] || error "missing configuration name"
[[ -n "$2" ]] || error "missing command ('setup', 'run', 'teardown')"

NAME=$1
FUNC=$2
shift 2

export NAME

# Default log level. This must be either Debug or Dump, as some tests
# rely on parsing output log messages to check that, for example, a
# particular authentication method has been actually used.

: "${XRD_LOGLEVEL:=Debug}"

# Write out client logs into a file. If the test fails, this file will
# be attached in CDash along with server logs to make debugging easier.

XRD_LOGFILE="${PWD}/${NAME}/client.log"

export XRD_LOGLEVEL XRD_LOGFILE

# Reduce default timeouts to catch errors quickly and prevent the test
# suite from getting stuck waiting for timeouts while running.

: "${XRD_REQUESTTIMEOUT:=2}"
: "${XRD_STREAMTIMEOUT:=2}"
: "${XRD_TIMEOUTRESOLUTION:=1}"

export XRD_REQUESTTIMEOUT XRD_STREAMTIMEOUT XRD_TIMEOUTRESOLUTION

# Source directory is the directory where this script is located
: "${SOURCE_DIR:="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"}"

# Prepend source directory to PATH
PATH="${SOURCE_DIR}:${PATH}"

# Binary directory is the root of a build directory, when run
# as part of a regular CMake build. In this case, we also add
# the build directories with binaries to PATH and check that
# XRootD commands really come from there.

if [[ -n "${BINARY_DIR}" ]]; then
	PATH="${BINARY_DIR}/src:${BINARY_DIR}/src/XrdCl:${PATH}"
	for PROG in cconfig cmsd xrootd xrdcp xrdfs xrdadler32; do
		if [[ ! "$(command -v "${PROG}")" =~ ${BINARY_DIR} ]]; then
			error "'${PROG}': not used from build directory"
		fi
	done
fi

# This is the configuration file that will be used to start the server

CONF="${SOURCE_DIR}/${NAME}.cfg"

# This is the actual test script. It must define a test_<name> function, and
# optionally can also define setup_<name>, and teardown_<name>. The functions
# setup_<name> and teardown_<name> will run before the main setup and after
# the main teardown, respectively.

SCRIPT="${SOURCE_DIR}/${NAME}.sh"

LOCAL_DIR="${PWD}/${NAME}/data"
REMOTE_DIR="${PWD}/${NAME}/xrootd"

export CONF PATH SOURCE_DIR BINARY_DIR LOCAL_DIR REMOTE_DIR

test -r "${SCRIPT}" || error "test script not found"

# shellcheck source=/dev/null
source  "${SCRIPT}" || error "failed to source ${SCRIPT}"

function printlogs() {
	tail -n "${MAXLINES:-20}" "${NAME}"/*.log 1>&2
}

function setup() {
	# Make sure to start with a fresh configuration
	[[ -d "${LOCAL_DIR}" ]] && teardown "${NAME}"

	mkdir -p "${LOCAL_DIR}" "${REMOTE_DIR}"

	if [[ $(type -t "setup_${NAME}") == "function" ]]; then
		"setup_${NAME}"
	fi

	echo "Server configuration: "
	if ! cconfig -x xrootd -c "${CONF}" 2>&1; then
		error "failed to parse configuration file: ${CONF}"
	fi

	if ! xrootd -b -l xrootd.log -s xrootd.pid -c "${CONF}" -n "${NAME}"; then
		printlogs "${NAME}"
		teardown "${NAME}"
		error "failed to start XRootD server"
	fi
}

function run() {
	# Extract server port from configuration file to avoid duplication
	XRD_PORT="$(cconfig -x xrootd -c "${CONF}" 2>&1 | grep xrd.port | tr -cd '0-9')"

	# Use the actual hostname if we have one, otherwise fallback to localhost
	HOST="root://${HOSTNAME:-localhost}:${XRD_PORT}/"

	export HOST XRD_PORT

	if [[ ! $(type -t "test_${NAME}") == "function" ]]; then
		error "required function not defined in ${SCRIPT}: test_${NAME}"
	fi

	test_"${NAME}" || (printlogs && exit 1)
}

function teardown() {
	[[ -d "${NAME}" ]] || exit 0
	pushd "${NAME}" >/dev/null || exit
	# Kill all processes that created pid files and are still running
	for PIDFILE in *.pid; do
		test -s "${PIDFILE}" || continue
		PID="$(ps -o pid= "$(cat "${PIDFILE}")")"
		if test -n "${PID}"; then
			kill -s TERM "${PID}"
		fi
	done
	popd >/dev/null || exit
	rm -rf "${NAME}"
	if [[ $(type -t "teardown_${NAME}") == "function" ]]; then
		"teardown_${NAME}"
	fi
}

# Make sure we clean up after ourselves on errors, Ctrl-C, etc
trap teardown ERR INT TERM QUIT ABRT

if [[ ! $(type -t "${FUNC}") == "function" ]]; then
	error "unknown command: ${FUNC}"
fi

"${FUNC}"
