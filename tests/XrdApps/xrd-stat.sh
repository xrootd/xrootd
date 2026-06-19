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

sum_help_out=$("${XRD}" sum --help)
for expected in \
  "xrd sum" \
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
  "--log-file" \
  "checksum_type"
do
  if ! grep -F -- "${expected}" <<< "${sum_help_out}" >/dev/null; then
    echo "xrd sum --help is missing '${expected}'" >&2
    exit 1
  fi
done

sum_version_out=$("${XRD}" sum --version)
if [[ "${sum_version_out}" != "${version_out}" ]]; then
  echo "xrd sum --version did not match xrd --version" >&2
  echo "xrd: ${version_out}" >&2
  echo "sum: ${sum_version_out}" >&2
  exit 1
fi

top_help_out=$("${XRD}" --help)
if ! grep -F -- "copy" <<< "${top_help_out}" >/dev/null; then
  echo "xrd --help is missing 'copy'" >&2
  exit 1
fi

for removed in \
  "legacy-bringonline" \
  "legacy-register" \
  "legacy-replicas" \
  "legacy-unregister"
do
  if grep -F -- "${removed}" <<< "${top_help_out}" >/dev/null; then
    echo "xrd --help should not list removed command '${removed}'" >&2
    exit 1
  fi
done

set +e
not_impl_err=$("${XRD}" cat "${file}" 2>&1 >/dev/null)
not_impl_rc=$?
set -e

if [[ ${not_impl_rc} -ne 2 ]]; then
  echo "xrd cat exit code was ${not_impl_rc}, expected 2" >&2
  echo "${not_impl_err}" >&2
  exit 1
fi

if ! grep -F -- "xrd cat: command is not implemented yet" \
  <<< "${not_impl_err}" >/dev/null; then
  echo "xrd cat did not print the not implemented error" >&2
  echo "${not_impl_err}" >&2
  exit 1
fi

copy_help_out=$("${XRD}" copy --help 2>&1)
for expected in \
  "Usage:   xrd copy" \
  "--force" \
  "--recursive" \
  "--cksum"
do
  if ! grep -F -- "${expected}" <<< "${copy_help_out}" >/dev/null; then
    echo "xrd copy --help is missing '${expected}'" >&2
    exit 1
  fi
done

copy_src="${tmpdir}/copy-source"
copy_dst="${tmpdir}/copy-destination"
printf 'copy me\n' > "${copy_src}"
"${XRD}" copy -f "${copy_src}" "${copy_dst}"
if [[ "$(<"${copy_dst}")" != "copy me" ]]; then
  echo "xrd copy did not copy the local file content" >&2
  exit 1
fi

sum_out=$("${XRD}" sum "${file}" adler32)
if [[ "${sum_out}" != "file://${expected_file} 084b021f" ]]; then
  echo "xrd sum adler32 output did not match gfal-sum style" >&2
  echo "${sum_out}" >&2
  exit 1
fi

sum_file_url_out=$("${XRD}" sum "file://${expected_file}" adler32)
if [[ "${sum_file_url_out}" != "file://${expected_file} 084b021f" ]]; then
  echo "xrd sum file:// adler32 output did not match gfal-sum style" >&2
  echo "${sum_file_url_out}" >&2
  exit 1
fi

sum_upper_out=$("${XRD}" sum "${file}" ADLER32)
if [[ "${sum_upper_out}" != "file://${expected_file} 084b021f" ]]; then
  echo "xrd sum ADLER32 output did not match gfal-sum style" >&2
  echo "${sum_upper_out}" >&2
  exit 1
fi

sum_crc32c_out=$("${XRD}" sum "${file}" crc32c)
if [[ "${sum_crc32c_out}" != "file://${expected_file} 353dd8be" ]]; then
  echo "xrd sum crc32c output did not match gfal-sum style" >&2
  echo "${sum_crc32c_out}" >&2
  exit 1
fi

sum_md5_out=$("${XRD}" sum "${file}" md5)
if [[ "${sum_md5_out}" != "file://${expected_file} b1946ac92492d2347c6235b4d2611184" ]]; then
  echo "xrd sum md5 output did not match gfal-sum style" >&2
  echo "${sum_md5_out}" >&2
  exit 1
fi

sum_crc32_out=$("${XRD}" sum "${file}" crc32)
if [[ "${sum_crc32_out}" != "file://${expected_file} 909783072" ]]; then
  echo "xrd sum crc32 output did not match gfal-sum style" >&2
  echo "${sum_crc32_out}" >&2
  exit 1
fi

"${XRD}" sum \
  -t5 \
  -Dclient/option=value \
  -E/tmp/nonexistent-cert \
  --key /tmp/nonexistent-key \
  -4 \
  -Ctest-client-info \
  --log-file "${tmpdir}/xrd-sum.log" \
  "${file}" adler32 >/dev/null

set +e
sum_missing_args_err=$("${XRD}" sum "${file}" 2>&1)
sum_missing_args_rc=$?
set -e

if [[ ${sum_missing_args_rc} -ne 64 ]]; then
  echo "xrd sum missing-args exit code was ${sum_missing_args_rc}, expected 64" >&2
  echo "${sum_missing_args_err}" >&2
  exit 1
fi

if ! grep -F -- "xrd sum: expected one file URL and checksum type" \
  <<< "${sum_missing_args_err}" >/dev/null; then
  echo "xrd sum missing-args error did not match gfal-sum style" >&2
  echo "${sum_missing_args_err}" >&2
  exit 1
fi

set +e
sum_missing_err=$("${XRD}" sum "${tmpdir}/missing" adler32 2>&1)
sum_missing_rc=$?
set -e

if [[ ${sum_missing_rc} -ne 2 ]]; then
  echo "xrd sum missing-file exit code was ${sum_missing_rc}, expected 2" >&2
  echo "${sum_missing_err}" >&2
  exit 1
fi

if ! grep -F -- \
  "xrd sum error: 2 (No such file or directory) - errno reported by local system call No such file or directory" \
  <<< "${sum_missing_err}" >/dev/null; then
  echo "xrd sum missing-file error did not match gfal-sum style" >&2
  echo "${sum_missing_err}" >&2
  exit 1
fi

set +e
sum_bad_type_err=$("${XRD}" sum "${file}" BADTYPE 2>&1)
sum_bad_type_rc=$?
set -e

if [[ ${sum_bad_type_rc} -ne 38 ]]; then
  echo "xrd sum bad checksum exit code was ${sum_bad_type_rc}, expected 38" >&2
  echo "${sum_bad_type_err}" >&2
  exit 1
fi

if ! grep -F -- \
  "xrd sum error: 38 (Function not implemented) - Checksum type BADTYPE not supported for local files" \
  <<< "${sum_bad_type_err}" >/dev/null; then
  echo "xrd sum bad checksum error did not match gfal-sum style" >&2
  echo "${sum_bad_type_err}" >&2
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

if [[ -n "${XRD_SUM_REMOTE_URL:-}" ]]; then
  remote_sum_type="${XRD_SUM_REMOTE_TYPE:-adler32}"
  remote_sum_out=$("${XRD}" sum "${XRD_SUM_REMOTE_URL}" "${remote_sum_type}")
  if ! grep -E "^${XRD_SUM_REMOTE_URL} [0-9A-Fa-f]+$" \
    <<< "${remote_sum_out}" >/dev/null; then
    echo "remote xrd sum output did not match gfal-sum style" >&2
    echo "${remote_sum_out}" >&2
    exit 1
  fi
fi
