#!/bin/bash

startdir="$(pwd)"
mkdir xrootdbuild
cd xrootdbuild

# build only client
# build python bindings
# set install prefix
# set the respective version of python
# replace the default BINDIR with a custom one that can be easily removed afterwards
#    (for the python bindings we don't want to install the binaries)
CMAKE_ARGS="-DPYPI_BUILD=TRUE -DXRDCL_LIB_ONLY=TRUE -DENABLE_PYTHON=TRUE -DCMAKE_INSTALL_PREFIX=$1/pyxrootd -DXRD_PYTHON_REQ_VERSION=$2 -DCMAKE_INSTALL_BINDIR=$startdir/xrootdbuild/bin"

if [ "$5" = "true" ]; then
  source /opt/rh/devtoolset-7/enable
fi

cmake_path=$4
$cmake_path .. $CMAKE_ARGS

res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

make -j8
res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

cd src
make install
res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

cd ../bindings/python
$6 setup.py install $3 # $6 holds the python sys.executable
res=$?
if [ "$res" -ne "0" ]; then
    exit 1
fi

cd $startdir
rm -r xrootdbuild

# convert the egg-info into a proper dist-info
egginfo_path=$(ls $1/xrootd-*.egg-info)
core="${egginfo_path%.*}"
core="${egginfo_path#$1/}"
sufix="${core#xrootd-*.*.*_-}"
core="${core%_-*}"
if [[ "$core" == "$sufix" ]]
then
    distinfo_path="${egginfo_path%.*}.dist-info"
else
    distinfo_path="$1/$core-$sufix"
fi
echo $distinfo_path >> /tmp/out.txt
mkdir $distinfo_path
mv $egginfo_path $distinfo_path/METADATA
echo -e "Wheel-Version: 1.0\nGenerator: bdist_wheel (0.35.1)\nRoot-Is-Purelib: true\nTag: py2-none-any\nTag: py3-none-any" > $distinfo_path/WHEEL
touch $distinfo_path/RECORD
distinfo_name=${distinfo_path#"$1"}
find $1/pyxrootd/      -type f -exec sha256sum {} \; | awk '{printf$2 ",sha256=" $1 "," ; system("stat --printf=\"%s\" "$2) ; print '\n'; }' >> $distinfo_path/RECORD
find $1/$distinfo_name -type f -exec sha256sum {} \; | awk '{printf$2 ",sha256=" $1 "," ; system("stat --printf=\"%s\" "$2) ; print '\n'; }' >> $distinfo_path/RECORD
find $1/XRootD/        -type f -exec sha256sum {} \; | awk '{printf$2 ",sha256=" $1 "," ; system("stat --printf=\"%s\" "$2) ; print '\n'; }' >> $distinfo_path/RECORD
find $1/pyxrootd/ -type l | awk '{print$1 ",,"}' >> $distinfo_path/RECORD
