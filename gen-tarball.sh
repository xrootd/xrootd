#!/bin/bash

# Generate a source tarball including submodules
if [ -z "${1}" ] ; then
    echo No tag or branch given
    exit 1
fi
ver=${1}
# Remove initial v from tag name for use in filenames
if [ ${ver:0:1} = 'v' ] ; then
    fver=${ver:1}
else
    fver=${ver}
fi
if [ -r xrootd-${fver}.tar.gz ] ; then
    echo xrootd-${fver}.tar.gz already exists
    exit 1
fi
curdir=$(pwd)
tdir=$(mktemp -d)
cd ${tdir}
git clone https://github.com/xrootd/xrootd.git
cd xrootd
git checkout ${ver}
if [ $? -ne 0 ] ; then
    echo No such tag or branch: ${ver}
    cd ${curdir}
    rm -rf ${tdir}
    exit 1
fi
git archive --prefix xrootd-${fver}/ ${ver} -o ${tdir}/xrootd-${fver}.tar
cd ${tdir}
gzip xrootd-${fver}.tar
mv xrootd-${fver}.tar.gz ${curdir}
cd ${curdir}
rm -rf ${tdir}

