#!/usr/bin/env bash
set -euo pipefail

python_version="${PYTHON_VERSION:-3.14}"
image="${UPROOT_CI_IMAGE:-python:${python_version}-bookworm}"
repo_root="$(git rev-parse --show-toplevel)"
workdir="$(mktemp -d "${TMPDIR:-/tmp}/xrootd-uproot-ci.XXXXXX")"

cleanup() {
  if [[ "${KEEP_UPROOT_CI_WORKDIR:-0}" == "1" ]]; then
    printf 'Keeping workdir: %s\n' "$workdir"
  else
    rm -rf "$workdir"
  fi
}
trap cleanup EXIT

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required to run the Uproot CI job locally" >&2
  exit 1
fi

git -C "$repo_root" archive --format=tar HEAD | tar -xf - -C "$workdir"

docker run --rm \
  -e "FSSPEC_XROOTD_VERSION=${FSSPEC_XROOTD_VERSION:-}" \
  -e "UPROOT_REF=${UPROOT_REF:-}" \
  -v "$workdir:/work" \
  -w /work \
  "$image" \
  bash -euxo pipefail -c '
    export DEBIAN_FRONTEND=noninteractive
    export CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release"
    export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-4}"

    apt-get update -qq
    apt-get install -y --no-install-recommends \
      build-essential \
      ca-certificates \
      cmake \
      git \
      krb5-multidev \
      libssl-dev \
      uuid-dev

    python -m pip install --upgrade pip
    python -m pip install build wheel
    git config --global --add safe.directory "$PWD"
    ./genversion.sh >| VERSION
    python -m pip wheel --use-pep517 --verbose --wheel-dir wheelhouse .

    cmake -S . -B build/server \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$PWD/xrootd-install" \
      -DENABLE_PYTHON=0 \
      -DENABLE_TESTS=0 \
      -DENABLE_FUSE=0 \
      -DENABLE_HTTP=0 \
      -DENABLE_VOMS=0 \
      -DENABLE_SCITOKENS=0 \
      -DENABLE_MACAROONS=0 \
      -DENABLE_XRDOSSARC=0
    cmake --build build/server --parallel "$CMAKE_BUILD_PARALLEL_LEVEL"
    cmake --install build/server
    export PATH="$PWD/xrootd-install/bin:$PATH"
    export LD_LIBRARY_PATH="$PWD/xrootd-install/lib:${LD_LIBRARY_PATH:-}"

    if [ -z "${UPROOT_REF}" ]; then
      python -m pip install uproot
      UPROOT_VERSION="$(python -m pip show uproot | awk "/^Version:/ { print \$2 }")"
      UPROOT_REF="tags/v${UPROOT_VERSION}"
    fi

    git clone https://github.com/scikit-hep/uproot5.git uproot
    git -C uproot checkout "${UPROOT_REF}"
    (cd uproot && python -m pip install . --group test)

    if [ -z "${FSSPEC_XROOTD_VERSION}" ]; then
      python -m pip install --upgrade fsspec-xrootd
    else
      python -m pip install --upgrade "fsspec-xrootd==${FSSPEC_XROOTD_VERSION}"
    fi

    python -m pip install --force-reinstall --no-deps wheelhouse/xrootd-*.whl
    command -v xrootd
    xrootd -v
    python -m pip show xrootd
    python -c "import XRootD; print(XRootD)"
    python -c "from XRootD import client; print(client.FileSystem(\"root://localhost\"))"

    python -m pytest -vv -k "xrootd" uproot/tests \
      --reruns 10 \
      --reruns-delay 30 \
      --only-rerun "(?i)OSError|FileNotFoundError|timeout|expired|connection|socket"
  '
