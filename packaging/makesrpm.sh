#!/bin/bash
#-------------------------------------------------------------------------------
# Create a source RPM package
# Author: Lukasz Janyst <ljanyst@cern.ch> (10.03.2011)
#-------------------------------------------------------------------------------

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
SOURCEPATH=$(realpath $(dirname $0)/..)
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
    VERSION="${2}"
    shift
  elif test x${1} = x--define; then
    if test ${#} -lt 2; then
      echo "--define parameter needs an argument" 1>&2
      exit 1
    fi
    USER_DEFINE="$USER_DEFINE --define \""${2}"\""
    shift
  elif test x${1} = x-D; then
    if test ${#} -lt 2; then
      echo "-D parameter needs an argument" 1>&2
      exit 1
    fi
    USER_D="$USER_D -D \""${2}"\""
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
if ! command -v rpmbuild 2>/dev/null; then
  echo "[!] Unable to find rpmbuild, aborting..." 1>&2
  exit 1
fi

if ! command -v git 2>/dev/null; then
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

: ${VERSION:=$(git --git-dir=$SOURCEPATH/.git describe)}
: ${RELEASE:=1}

if test $? -ne 0; then
  echo "[!] Unable to figure out the version number" 1>&2
  exit 4
fi

echo "[i] Working with version: $VERSION"

#-------------------------------------------------------------------------------
# Sanitize version to work with RPMs
# https://docs.fedoraproject.org/en-US/packaging-guidelines/Versioning/
#-------------------------------------------------------------------------------

VERSION=${VERSION#v} # remove "v" prefix
VERSION=${VERSION/-rc/~rc} # release candidates use ~ in RPMs
VERSION=${VERSION/-g*/} # snapshots versions not supported well, filter out
VERSION=${VERSION/-/.post} # handle git describe for post releases
VERSION=${VERSION//-/.} # replace remaining dashes with dots

echo "[i] RPM compliant version: $VERSION-$RELEASE"

#-------------------------------------------------------------------------------
# Create a tempdir and copy the files there
#-------------------------------------------------------------------------------
# exit on any error
set -e

TEMPDIR=`mktemp -d /tmp/xrootd.srpm.XXXXXXXXXX`
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
if test ! -r rhel/xrootd.spec.in; then
  echo "[!] The specfile template does not exist!" 1>&2
  exit 7
fi
cat rhel/xrootd.spec.in | sed "s/__VERSION__/$VERSION/" | \
  sed "s/__RELEASE__/$RELEASE/" > $TEMPDIR/xrootd.spec

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

git describe --tags --abbrev=0 --exact-match >/dev/null 2>&1
if test ${?} -eq 0; then
  TAG="`git describe --tags --abbrev=0 --exact-match`"
fi

git archive --prefix=xrootd/ --format=tar $COMMIT > $RPMSOURCES/xrootd.tar

if test $? -ne 0; then
  echo "[!] Unable to create the source tarball" 1>&2
  exit 6
fi

#-------------------------------------------------------------------------------
# gzip the tarball
#-------------------------------------------------------------------------------
gzip -9fn $RPMSOURCES/xrootd.tar

#-------------------------------------------------------------------------------
# Check if we need some other versions
#-------------------------------------------------------------------------------
OTHER_VERSIONS=`cat $TEMPDIR/xrootd.spec | \
    grep -E '^Source[0-9]+:[[:space:]]*xrootd-.*.gz$' |\
    awk  '{ print $2; }'`

for VER in $OTHER_VERSIONS; do
    VER=${VER/xrootd-/}
    VER=${VER/.tar.gz/}

    git archive --prefix=xrootd-$VER/ --format=tar v$VER | gzip -9fn > \
        $RPMSOURCES/xrootd-$VER.tar.gz

    if test $? -ne 0; then
        echo "[!] Unable to create the source tarball" 1>&2
        exit 9
    fi
done
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
               ${USER_D} \
               -bs $TEMPDIR/xrootd.spec > $TEMPDIR/log"
if test $? -ne 0; then
  echo "[!] RPM creation failed" 1>&2
  exit 8
fi

cp $TEMPDIR/rpmbuild/SRPMS/xrootd*.src.rpm $OUTPUTPATH
rm -rf $TEMPDIR

echo "[i] Done."
