#!/usr/bin/env bash

set -euo pipefail

: "${XRD:?XRD must point to the xrd executable}"

tmpdir=$(mktemp -d)
trap 'rm -rf "${tmpdir}"' EXIT

file="${tmpdir}/stat-file"
printf 'hello\n' > "${file}"
chmod 600 "${file}"
expected_file="$(cd "$(dirname "${file}")" && pwd -P)/$(basename "${file}")"

exec_file="${tmpdir}/stat-executable"
printf '#!/bin/sh\nexit 0\n' > "${exec_file}"
chmod 755 "${exec_file}"
expected_exec_file="$(cd "$(dirname "${exec_file}")" && pwd -P)/$(basename "${exec_file}")"

dir="${tmpdir}/stat-directory"
mkdir "${dir}"
chmod 755 "${dir}"
expected_dir="$(cd "${dir}" && pwd -P)"

version_out=$("${XRD}" --version)
if ! grep -E '^xrd v?[0-9]' <<< "${version_out}" >/dev/null; then
  echo "xrd --version did not print a version string" >&2
  echo "${version_out}" >&2
  exit 1
fi

help_out=$("${XRD}" stat --help)
for expected in \
  "xrd stat" \
  "-V" \
  "--version" \
  "-D" \
  "--definition" \
  "-t" \
  "--timeout" \
  "-E" \
  "--key" \
  "-4" \
  "-6" \
  "-C" \
  "--client-info" \
  "--log-file"
do
  if ! grep -F -- "${expected}" <<< "${help_out}" >/dev/null; then
    echo "xrd stat --help is missing '${expected}'" >&2
    exit 1
  fi
done

stat_version_out=$("${XRD}" stat --version)
if [[ "${stat_version_out}" != "${version_out}" ]]; then
  echo "xrd stat --version did not match xrd --version" >&2
  echo "xrd: ${version_out}" >&2
  echo "stat: ${stat_version_out}" >&2
  exit 1
fi

stat_out=$("${XRD}" stat "${file}")
case "${stat_out}" in
  *"  File: 'file://${expected_file}'"*);;
  *)
    echo "xrd stat did not print the local file as a file:// URL" >&2
    echo "${stat_out}" >&2
    exit 1
    ;;
esac

for expected in \
  $'  Size: 6\tregular file' \
  $'Access: (0600/-rw-------)' \
  "Access: " \
  "Modify: " \
  "Change: "
do
  if ! grep -F -- "${expected}" <<< "${stat_out}" >/dev/null; then
    echo "xrd stat output is missing '${expected}'" >&2
    echo "${stat_out}" >&2
    exit 1
  fi
done

stat_file_url_out=$("${XRD}" stat "file://${expected_file}")
if [[ "${stat_file_url_out}" != "${stat_out}" ]]; then
  echo "xrd stat file:// output did not match plain local path output" >&2
  echo "plain path:" >&2
  echo "${stat_out}" >&2
  echo "file URL:" >&2
  echo "${stat_file_url_out}" >&2
  exit 1
fi

stat_exec_out=$("${XRD}" stat "${exec_file}")
for expected in \
  "  File: 'file://${expected_exec_file}'" \
  $'  Size: 17\tregular file' \
  $'Access: (0755/-rwxr-xr-x)'
do
  if ! grep -F -- "${expected}" <<< "${stat_exec_out}" >/dev/null; then
    echo "xrd stat executable output is missing '${expected}'" >&2
    echo "${stat_exec_out}" >&2
    exit 1
  fi
done

stat_dir_out=$("${XRD}" stat "${dir}")
for expected in \
  "  File: 'file://${expected_dir}'" \
  "directory" \
  $'Access: (0755/drwxr-xr-x)' \
  "Access: " \
  "Modify: " \
  "Change: "
do
  if ! grep -F -- "${expected}" <<< "${stat_dir_out}" >/dev/null; then
    echo "xrd stat directory output is missing '${expected}'" >&2
    echo "${stat_dir_out}" >&2
    exit 1
  fi
done

"${XRD}" stat \
  -t5 \
  -Dclient/option=value \
  -E/tmp/nonexistent-cert \
  --key /tmp/nonexistent-key \
  -4 \
  -Ctest-client-info \
  --log-file "${tmpdir}/xrd.log" \
  "${file}" >/dev/null

set +e
stat_missing_args_err=$("${XRD}" stat 2>&1)
stat_missing_args_rc=$?
set -e

if [[ ${stat_missing_args_rc} -ne 64 ]]; then
  echo "xrd stat missing-args exit code was ${stat_missing_args_rc}, expected 64" >&2
  echo "${stat_missing_args_err}" >&2
  exit 1
fi

if ! grep -F -- "xrd stat: expected one file URL" \
  <<< "${stat_missing_args_err}" >/dev/null; then
  echo "xrd stat missing-args error did not match gfal-stat style" >&2
  echo "${stat_missing_args_err}" >&2
  exit 1
fi

set +e
missing_err=$("${XRD}" stat "${tmpdir}/missing" 2>&1)
missing_rc=$?
set -e

if [[ ${missing_rc} -ne 2 ]]; then
  echo "xrd stat missing-file exit code was ${missing_rc}, expected 2" >&2
  echo "${missing_err}" >&2
  exit 1
fi

if ! grep -F -- \
  "xrd stat error: 2 (No such file or directory) - errno reported by local system call No such file or directory" \
  <<< "${missing_err}" >/dev/null; then
  echo "xrd stat missing-file error did not match gfal-stat style" >&2
  echo "${missing_err}" >&2
  exit 1
fi

if [[ -n "${XRD_STAT_REMOTE_URL:-}" ]]; then
  remote_out=$("${XRD}" stat "${XRD_STAT_REMOTE_URL}")
  for expected in "  File: '${XRD_STAT_REMOTE_URL}'" "  Size: " "Access: " "Modify: " "Change: "
  do
    if ! grep -F -- "${expected}" <<< "${remote_out}" >/dev/null; then
      echo "remote xrd stat output is missing '${expected}'" >&2
      echo "${remote_out}" >&2
      exit 1
    fi
  done
fi

