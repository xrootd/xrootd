#!/bin/bash

set -e

TAG="$(printf "%s" "$(git describe "${1:-HEAD}")")"
NAME="xrootd-${TAG#v}"

set -x

git archive -9 --prefix="${NAME}/" -o "${NAME}.tar.gz" "${TAG}"

