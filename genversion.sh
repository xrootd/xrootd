#!/usr/bin/env bash

# This script no longer generates XrdVersion.hh, but just
# prints the version using the same strategy as in the module
# XRootDVersion.cmake. The script will first try to use a custom
# version set with the option --version or via the environment
# variable XRDVERSION, then read the VERSION file and if that is
# not expanded by git, git describe is used. If a bad version is
# set for any reason, a fallback version is used based on a date.

function usage()
{
	cat 1>&2 <<-EOF
		Usage:
			$(basename "$0") [--help|--version]

		--help       prints this message
		--print-only ignored, used for backward compatibility
		--sanitize   sanitize version for use with package managers
		--version    VERSION sets a custom version
	EOF
}

SRC="$(dirname "$0")"
VF="${SRC}/VERSION"

if [[ -n "${XRDVERSION}" ]]; then
	VERSION="${XRDVERSION}";
elif [[ -r "${VF}" ]] && grep -vq "Format:" "${VF}"; then
	VERSION="$(sed -e 's/-g/+git/' "${VF}")"
elif git -C "${SRC}" describe --match 'v*' >/dev/null 2>&1; then
	VERSION="$(git -C "${SRC}" describe --match 'v*' | sed -e 's/-g/+git/')"
else
	VERSION="v5.7-rc$(date +%Y%m%d)"
fi

while [[ $# -gt 0 ]]; do
	case $1 in
	--help)
		usage
		exit 0
	;;

	--print-only)
		shift
	;;

	--sanitize)
		SANITIZE=1
		shift
	;;

	--version)
		shift
		if [[ $# == 0 ]]; then
			echo "error: --version parameter needs an argument" 1>&2
		fi
		VERSION=$1
		shift
	;;

	*)
		echo "warning: unknown option: $1" 1>&2
		shift
	;;
	esac
done

if [[ -v SANITIZE ]]; then
	VERSION=$(sed -e 's/v//; s/-rc/~rc/; s/-g/+git/; s/-/.post/; s/-/./' <<< "${VERSION}")
fi

printf "%s" "${VERSION}"
