#!/bin/bash

VERSION=0.1.0
NAME=xrootd-python-$VERSION
SOURCES=$NAME.tar.gz

ARCHIVE_DEST=$PWD
DEST=$ARCHIVE_DEST/$NAME
SRC=..

set -e

mkdir -p $DEST

cp -R $SRC/setup.py $DEST
cp -R $SRC/src $DEST
cp -R $SRC/libs $DEST
cp -R $SRC/tests $DEST
# cp -R $SRC/docs $DEST
cp -R $SRC/examples $DEST

tar -czf $SOURCES $NAME
rm -rf $DEST

TEMPDIR=`mktemp -d /tmp/pyxrootd.srpm.XXXXXXXXXX`
RPMSOURCES=$TEMPDIR/rpmbuild/SOURCES
mkdir -p $RPMSOURCES
mkdir -p $TEMPDIR/rpmbuild/SRPMS

mv $SOURCES $RPMSOURCES

echo "[i] Creating the source RPM..."

# Dirty, dirty hack!
echo "%_sourcedir $RPMSOURCES" >> $TEMPDIR/rpmmacros
rpmbuild --define "_topdir $TEMPDIR/rpmbuild"    \
         --define "%_sourcedir $RPMSOURCES"      \
         --define "%_srcrpmdir %{_topdir}/SRPMS" \
         --define "_source_filedigest_algorithm md5" \
         --define "_binary_filedigest_algorithm md5" \
  -bs rhel/pyxrootd.spec > $TEMPDIR/log
if test $? -ne 0; then
  echo "[!] RPM creation failed" 1>&2
  exit 1
fi

cp $TEMPDIR/rpmbuild/SRPMS/xrootd-python*.src.rpm $PWD
rm -rf $TEMPDIR

echo "[i] Done."