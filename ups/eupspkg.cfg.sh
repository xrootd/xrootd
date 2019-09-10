# EupsPkg config file. Sourced by 'eupspkg'

# Breaks on Darwin w/o this
export LANG=C

PKGDIR=$PWD
BUILDDIR=$PWD/../xrootd-build

config()
{
        rm -rf ${BUILDDIR}
        mkdir ${BUILDDIR}
        cd ${BUILDDIR}
        cmake ${PKGDIR} -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=${PREFIX} -DENABLE_PYTHON=FALSE
}

build()
{
        cd ${BUILDDIR}
        default_build
}

install()
{
        cd ${BUILDDIR}
        make -j$NJOBS install

        ARCH=`arch`
        case "${ARCH}" in
            x86_64) mkdir -p ${PREFIX}/lib && cd ${PREFIX}/lib && ln -s ../lib64/* . ;;
            *)      echo "Default behaviour for managing lib(64)/ directory" ;;
        esac


        cd ${PKGDIR}
        install_ups
}
