#!/bin/bash

#-------------------------------------------------------------------------------
# Process the git decoration expansion and try to derive version number
#-------------------------------------------------------------------------------
function getVersionFromRefs()
{
  REFS=${1/RefNames:/}
  REFS=${REFS//,/}
  REFS=${REFS/(/}
  REFS=${REFS/)/}
  REFS=($REFS)
  if test ${#REFS[@]} -eq 3; then
    echo "${REFS[1]}"
  else
    echo "unknown"
  fi
}

#-------------------------------------------------------------------------------
# We're not inside a git repo
#-------------------------------------------------------------------------------
if test ! -d .git; then
  #-----------------------------------------------------------------------------
  # We cannot figure out what version we are
  #----------------------------------------------------------------------------
  echo "[I] No git repository info found. Trying to interpret VERSION_INFO"
  if test ! -r VERSION_INFO; then
    echo "[!] VERSION_INFO file absent. Unable to determine the version. Using \"unknown\""
    VERSION="unknown"
  elif test x"`grep Format VERSION_INFO`" != x; then
    echo "[!] VERSION_INFO file invalid. Unable to determine the version. Using \"unknown\""
    VERSION="unknown"

  #-----------------------------------------------------------------------------
  # The version file exists and seems to be valid so we know the version
  #----------------------------------------------------------------------------
  else
    REFNAMES="`grep RefNames VERSION_INFO`"
    VERSION="`getVersionFromRefs "$REFNAMES"`"
    if test x$VERSION == xunknown; then
      SHORTHASH="`grep ShortHash VERSION_INFO`"
      SHORTHASH=${SHORTHASH/ShortHash:/}
      SHORTHASH=${SHORTHASH// /}
      VERSION="untagged-$SHORTHASH"
    fi
  fi

#-------------------------------------------------------------------------------
# We're in a git repo so we can try to determine the version using that
#-------------------------------------------------------------------------------
else
  echo "[I] Determining version from git"
  GIT=`which git 2>/dev/null`
  if test ! -x ${GIT}; then
    echo "[!] Unable to find git in the path: setting the version tag to unknown"
    VERSION="unknown"
  fi

  git describe --tags --abbrev=0 --exact-match >/dev/null 2>&1

  if test ${?} -eq 0; then
    VERSION="`git describe --tags --abbrev=0 --exact-match`"
  else
    VERSION="`git describe --tags --abbrev=0`+more"
  fi
fi

#-------------------------------------------------------------------------------
# Create XrdVersion.hh
#-------------------------------------------------------------------------------
if test ! -r src/XrdVersion.hh.in; then
   echo "[!] Unable to find src/XrdVersion.hh.in"
   exit 1
fi

sed -e "s/#define XrdVERSION  \"unknown\"/#define XrdVERSION  \"$VERSION\"/" src/XrdVersion.hh.in > src/XrdVersion.hh.new

if test $? -ne 0; then
  echo "[!] Error while generating src/XrdVersion.hh from the input template"
  exit 1
fi

if test ! -e src/XrdVersion.hh; then
  mv src/XrdVersion.hh.new src/XrdVersion.hh
elif test x"`diff src/XrdVersion.hh.new src/XrdVersion.hh`" != x; then
    mv src/XrdVersion.hh.new src/XrdVersion.hh
fi
echo "[I] src/XrdVersion.hh successfuly generated"
