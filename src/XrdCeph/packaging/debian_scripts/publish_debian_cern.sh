#!/bin/bash

#-------------------------------------------------------------------------------
# Publish debian artifacts on CERN Gitlab CI
# Author: Jozsef Makai <jmakai@cern.ch> (11.08.2017)
#-------------------------------------------------------------------------------

set -e

comp=$1
prefix=/eos/project/s/storage-ci/www/debian/xrootd

for dist in artful xenial; do
  echo "Publishing for $dist";
  path=$prefix/pool/$dist/$comp/x/xrootd/;
  mkdir -p $path;
  if [[ "$comp" == "master" ]]; then find ${path} -type f -name '*deb' -delete; fi
  cp $dist/*deb $path;
  mkdir -p $prefix/dists/$dist/$comp/binary-amd64/;
  (cd $prefix && apt-ftparchive --arch amd64 packages pool/$dist/$comp/ > dists/$dist/$comp/binary-amd64/Packages);
  gzip -c $prefix/dists/$dist/$comp/binary-amd64/Packages > $prefix/dists/$dist/$comp/binary-amd64/Packages.gz;
  components=$(find $prefix/dists/$dist/ -mindepth 1 -maxdepth 1 -type d -exec basename {} \; | tr '\n' ' ')
  if [ -e $prefix/dists/$dist/Release ]; then
    rm $prefix/dists/$dist/Release
  fi
  if [ -e $prefix/dists/$dist/InRelease ]; then
    rm $prefix/dists/$dist/InRelease
  fi
  if [ -e $prefix/dists/$dist/Release.gpg ]; then
    rm $prefix/dists/$dist/Release.gpg
  fi
  apt-ftparchive -o APT::FTPArchive::Release::Origin=CERN -o APT::FTPArchive::Release::Label=XrootD -o APT::FTPArchive::Release::Codename=artful -o APT::FTPArchive::Release::Architectures=amd64 -o APT::FTPArchive::Release::Components="$components" release $prefix/dists/$dist/ > $prefix/dists/$dist/Release;
  gpg --homedir /home/stci/ --clearsign -o $prefix/dists/$dist/InRelease $prefix/dists/$dist/Release;
  gpg --homedir /home/stci/ -abs -o $prefix/dists/$dist/Release.gpg $prefix/dists/$dist/Release;
done
