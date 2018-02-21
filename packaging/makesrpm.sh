#!/bin/bash
#-------------------------------------------------------------------------------
# Create a source RPM package
# Author: Lukasz Janyst <ljanyst@cern.ch> (10.03.2011)
#-------------------------------------------------------------------------------

RCEXP='^[0-9]+\.[0-9]+\.[0-9]+\-rc.*$'
CERNEXP='^[0-9]+\.[0-9]+\.[0-9]+\-[0-9]+\.CERN.*$'

#-------------------------------------------------------------------------------
# Find a program
#-------------------------------------------------------------------------------
function findProg()
{
  for prog in $@; do
    if test -x "`which $prog 2>/dev/null`"; then
      echo $prog
      break
    fi
  done
}

#-------------------------------------------------------------------------------
# Print help
#-------------------------------------------------------------------------------
function printHelp()
{
  echo "Usage:"                                               1>&2
  echo "${0} [--help] [--source PATH] [--output PATH]"        1>&2
  echo "  --help        prints this message"                  1>&2
  echo "  --source  PATH specify the root of the source tree" 1>&2
  echo "                defaults to ../"                      1>&2
  echo "  --output  PATH the directory where the source rpm"  1>&2
  echo "                should be stored, defaulting to ."    1>&2
  echo "  --version VERSION the version provided by user"     1>&2
  echo "  --define  'MACRO EXPR'"                             1>&2
}

#-------------------------------------------------------------------------------
# Parse the commandline, if only we could use getopt... :(
#-------------------------------------------------------------------------------
SOURCEPATH="../"
OUTPUTPATH="."
PRINTHELP=0

while test ${#} -ne 0; do
  if test x${1} = x--help; then
    PRINTHELP=1
  elif test x${1} = x--source; then
    if test ${#} -lt 2; then
      echo "--source parameter needs an argument" 1>&2
      exit 1
    fi
    SOURCEPATH=${2}
    shift
  elif test x${1} = x--output; then
    if test ${#} -lt 2; then
      echo "--output parameter needs an argument" 1>&2
      exit 1
    fi
    OUTPUTPATH=${2}
    shift
  elif test x${1} = x--version; then
    if test ${#} -lt 2; then
      echo "--version parameter needs an argument" 1>&2
      exit 1
    fi
    USER_VERSION="--version ${2}"
    shift
  elif test x${1} = x--define; then
    if test ${#} -lt 2; then
      echo "--define parameter needs an argument" 1>&2
      exit 1
    fi
    USER_DEFINE="$USER_DEFINE --define \""${2}"\""
    shift
  fi
  shift
done

if test $PRINTHELP -eq 1; then
  printHelp
  exit 0
fi

echo "[i] Working on: $SOURCEPATH"
echo "[i] Storing the output to: $OUTPUTPATH"

#-------------------------------------------------------------------------------
# Check if the source and the output dirs
#-------------------------------------------------------------------------------
if test ! -d $SOURCEPATH -o ! -r $SOURCEPATH; then
  echo "[!] Source path does not exist or is not readable" 1>&2
  exit 2
fi

if test ! -d $OUTPUTPATH -o ! -w $OUTPUTPATH; then
  echo "[!] Output path does not exist or is not writeable" 1>&2
  exit 2
fi

#-------------------------------------------------------------------------------
# Check if we have all the necassary components
#-------------------------------------------------------------------------------
if test x`findProg rpmbuild` = x; then
  echo "[!] Unable to find rpmbuild, aborting..." 1>&2
  exit 1
fi

if test x`findProg git` = x; then
  echo "[!] Unable to find git, aborting..." 1>&2
  exit 1
fi

#-------------------------------------------------------------------------------
# Check if the source is a git repository
#-------------------------------------------------------------------------------
if test ! -d $SOURCEPATH/.git; then
  echo "[!] I can only work with a git repository" 1>&2
  exit 2
fi

#-------------------------------------------------------------------------------
# Check the version number
#-------------------------------------------------------------------------------
if test ! -x $SOURCEPATH/genversion.sh; then
  echo "[!] Unable to find the genversion script" 1>&2
  exit 3
fi

VERSION=`$SOURCEPATH/genversion.sh --print-only $USER_VERSION $SOURCEPATH 2>/dev/null`
if test $? -ne 0; then
  echo "[!] Unable to figure out the version number" 1>&2
  exit 4
fi

echo "[i] Working with version: $VERSION"

if test x${VERSION:0:1} = x"v"; then
  VERSION=${VERSION:1}
fi

#-------------------------------------------------------------------------------
# Deal with release candidates
#-------------------------------------------------------------------------------
RELEASE=1
if test x`echo $VERSION | egrep $RCEXP` != x; then
  RELEASE=0.`echo $VERSION | sed 's/.*-rc/rc/'`
  VERSION=`echo $VERSION | sed 's/-rc.*//'`
fi

#-------------------------------------------------------------------------------
# Deal with CERN releases
#-------------------------------------------------------------------------------
if test x`echo $VERSION | egrep $CERNEXP` != x; then
  RELEASE=`echo $VERSION | sed 's/.*-//'` 
  VERSION=`echo $VERSION | sed 's/-.*\.CERN//'`
fi

#-------------------------------------------------------------------------------
# In case of user version check if the release number has been provided
#-------------------------------------------------------------------------------
if test x"$USER_VERSION" != x; then
  TMP=`echo $VERSION | sed 's#.*-##g'`
  if test $TMP != $VERSION; then
    RELEASE=$TMP
    VERSION=`echo $VERSION | sed 's#-[^-]*$##'`
  fi
fi

VERSION=`echo $VERSION | sed 's/-/./g'`
echo "[i] RPM compliant version: $VERSION-$RELEASE"

#-------------------------------------------------------------------------------
# Create a tempdir and copy the files there
#-------------------------------------------------------------------------------
# exit on any error
set -e

TEMPDIR=`mktemp -d /tmp/xrootd-ceph.srpm.XXXXXXXXXX`
RPMSOURCES=$TEMPDIR/rpmbuild/SOURCES
mkdir -p $RPMSOURCES
mkdir -p $TEMPDIR/rpmbuild/SRPMS

echo "[i] Working in: $TEMPDIR" 1>&2

if test -d rhel -a -r rhel; then
  for i in rhel/*; do
    cp $i $RPMSOURCES
  done
fi

if test -d common -a -r common; then
  for i in common/*; do
    cp $i $RPMSOURCES
  done
fi

#-------------------------------------------------------------------------------
# Generate the spec file
#-------------------------------------------------------------------------------
if test ! -r rhel/xrootd-ceph.spec.in; then
  echo "[!] The specfile template does not exist!" 1>&2
  exit 7
fi
cat rhel/xrootd-ceph.spec.in | sed "s/__VERSION__/$VERSION/" | \
  sed "s/__RELEASE__/$RELEASE/" > $TEMPDIR/xrootd-ceph.spec

#-------------------------------------------------------------------------------
# Make a tarball of the latest commit on the branch
#-------------------------------------------------------------------------------
# no more exiting on error
set +e

CWD=$PWD
cd $SOURCEPATH
COMMIT=`git log --pretty=format:"%H" -1`

if test $? -ne 0; then
  echo "[!] Unable to figure out the git commit hash" 1>&2
  exit 5
fi

git archive --prefix=xrootd-ceph/ --format=tar $COMMIT | gzip -9fn > \
      $RPMSOURCES/xrootd-ceph.tar.gz

if test $? -ne 0; then
  echo "[!] Unable to create the source tarball" 1>&2
  exit 6
fi

cd $CWD

#-------------------------------------------------------------------------------
# Build the source RPM
#-------------------------------------------------------------------------------
echo "[i] Creating the source RPM..."

# Dirty, dirty hack!
echo "%_sourcedir $RPMSOURCES" >> $TEMPDIR/rpmmacros
eval "rpmbuild --define \"_topdir $TEMPDIR/rpmbuild\"    \
               --define \"%_sourcedir $RPMSOURCES\"      \
               --define \"%_srcrpmdir %{_topdir}/SRPMS\" \
               --define \"_source_filedigest_algorithm md5\" \
               --define \"_binary_filedigest_algorithm md5\" \
               ${USER_DEFINE} \
               -bs $TEMPDIR/xrootd-ceph.spec > $TEMPDIR/log"
if test $? -ne 0; then
  echo "[!] RPM creation failed" 1>&2
  exit 8
fi

cp $TEMPDIR/rpmbuild/SRPMS/xrootd-ceph*.src.rpm $OUTPUTPATH
rm -rf $TEMPDIR

echo "[i] Done."
