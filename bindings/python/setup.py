import os
import platform
import subprocess
import sys

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from subprocess import check_call, check_output

try:
    from shutil import which
except ImportError:
    from distutils.spawn import find_executable as which

def get_cmake_args():
    args = os.getenv('CMAKE_ARGS')

    if not args:
        return []

    from shlex import split
    return split(args)

srcdir = '${CMAKE_CURRENT_SOURCE_DIR}'

cmdline_args = []

# Check for unexpanded srcdir to determine if this is part
# of a regular CMake build or a Python build using setup.py.

if not srcdir.startswith('$'):
    # When building the Python bindings as part of a standard CMake build,
    # propagate down which cmake command to use, and the build type, C++
    # compiler, build flags, and how to link libXrdCl from the main build.

    cmake = '${CMAKE_COMMAND}'

    cmdline_args += [
        '-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}',
        '-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}',
        '-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}',
        '-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}',
        '-DXRootD_CLIENT_LIBRARY=${CMAKE_BINARY_DIR}/src/XrdCl/libXrdCl${CMAKE_SHARED_LIBRARY_SUFFIX}',
        '-DXRootD_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/src;${CMAKE_BINARY_DIR}/src',
    ]
else:
    srcdir = '.'

    cmake = which("cmake3") or which("cmake")

    cmdline_args += get_cmake_args()

def get_version():
    version = '${XRootD_VERSION_STRING}'

    if version.startswith('$'):
        try:
            version = open('VERSION').read().strip()

            if version.startswith('$'):
                output = check_output(['git', 'describe'])
                version = output.decode().strip()
        except:
            version = None
            pass

    if version is None:
        from datetime import date
        version = '5.7-rc' + date.today().strftime("%Y%m%d")

    if version.startswith('v'):
        version = version[1:]

    # Sanitize version to conform to PEP 440
    # https://www.python.org/dev/peps/pep-0440
    version = version.replace('-rc', 'rc')
    version = version.replace('-g', '+git.')
    version = version.replace('-', '.post', 1)
    version = version.replace('-', '.')

    return version

class CMakeExtension(Extension):
    def __init__(self, name, src=srcdir, sources=[], **kwa):
        Extension.__init__(self, name, sources=sources, **kwa)
        self.src = os.path.abspath(src)

class CMakeBuild(build_ext):
    def build_extensions(self):
        if cmake is None:
            raise RuntimeError('Cannot find CMake executable')

        for ext in self.extensions:
            extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

            # Use relative RPATHs to ensure the correct libraries are picked up.
            # The RPATH below covers most cases where a non-standard path is
            # used for installation. It allows to find libXrdCl with a relative
            # path from the site-packages directory. Build with install RPATH
            # because libraries are installed by Python/pip not CMake, so CMake
            # cannot fix the install RPATH later on.

            cmake_args = [
                '-DPython_EXECUTABLE={}'.format(sys.executable),
                '-DCMAKE_BUILD_WITH_INSTALL_RPATH=TRUE',
                '-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY={}/{}'.format(self.build_temp, ext.name),
                '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={}/{}'.format(extdir, ext.name),
            ]

            if sys.platform == 'darwin':
                cmake_args += [ '-DCMAKE_INSTALL_RPATH=@loader_path/../../..' ]
            else:
                cmake_args += [ '-DCMAKE_INSTALL_RPATH=$ORIGIN/../../../../$LIB' ]

            cmake_args += cmdline_args

            if not os.path.exists(self.build_temp):
                os.makedirs(self.build_temp)

            check_call([cmake, ext.src, '-B', self.build_temp] + cmake_args)
            check_call([cmake, '--build', self.build_temp, '--parallel'])

version = get_version()

setup(name='xrootd',
      version=version,
      description='XRootD Python bindings',
      author='XRootD Developers',
      author_email='xrootd-dev@slac.stanford.edu',
      url='http://xrootd.org',
      download_url='https://github.com/xrootd/xrootd/archive/v%s.tar.gz' % version,
      keywords=['XRootD', 'network filesystem'],
      license='LGPLv3+',
      long_description=open(srcdir + '/README.md').read(),
      long_description_content_type='text/plain',
      packages = ['XRootD', 'XRootD.client', 'pyxrootd'],
      package_dir = {
        'pyxrootd'     : srcdir + '/src',
        'XRootD'       : srcdir + '/libs',
        'XRootD/client': srcdir + '/libs/client',
      },
      ext_modules= [ CMakeExtension('pyxrootd') ],
      cmdclass={ 'build_ext': CMakeBuild },
      zip_safe=False,
      classifiers=[
          "Intended Audience :: Information Technology",
          "Intended Audience :: Science/Research",
          "License :: OSI Approved :: GNU Lesser General Public License v3 or later (LGPLv3+)",
          "Operating System :: MacOS",
          "Operating System :: POSIX :: Linux",
          "Operating System :: Unix",
          "Programming Language :: C++",
          "Programming Language :: Python",
      ]
     )
