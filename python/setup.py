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
    # As part of a standard CMake build, the extension module has already been
    # compiled by the enclosing build, which knows how to link it against the
    # client library and tracks its dependencies. All that is left to do here
    # is to pick it up from where CMake left it and package it into a wheel.

    prebuilt = '${CMAKE_CURRENT_BINARY_DIR}/extension'

    cmake = None
else:
    prebuilt = None

    srcdir = '.'

    cmake = which("cmake3") or which("cmake")

    cmdline_args += get_cmake_args()

def get_version():
    version = '${XRootD_VERSION_STRING}'

    if version.startswith('$'):
        try:
            with open('VERSION') as f:
                version = f.read().strip()

            if version.startswith('$'):
                output = check_output(['git', 'describe'])
                version = output.decode().strip()
        except:
            version = None

    if version is None:
        from datetime import date
        version = '5.9-rc' + date.today().strftime("%Y%m%d")

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
        for ext in self.extensions:
            extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
            destdir = os.path.join(extdir, ext.name)

            if prebuilt is not None:
                self.mkpath(destdir)

                for entry in sorted(os.listdir(prebuilt)):
                    self.copy_file(os.path.join(prebuilt, entry), destdir)

                continue

            if cmake is None:
                raise RuntimeError('Cannot find CMake executable')

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
                '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={}'.format(destdir),
            ]

            if sys.platform == 'darwin':
                cmake_args += [ '-DCMAKE_INSTALL_RPATH=@loader_path/../../..' ]
            else:
                cmake_args += [ '-DCMAKE_INSTALL_RPATH=$ORIGIN/../../../../$LIB' ]

            cmake_args += cmdline_args

            if not os.path.exists(self.build_temp):
                os.makedirs(self.build_temp)

            check_call([cmake, ext.src, '-B', self.build_temp] + cmake_args)
            check_call([cmake, '--build', self.build_temp])

version = get_version()

setup(name='xrootd',
      version=version,
      description='XRootD Python bindings',
      author='XRootD Developers',
      author_email='xrootd-dev@slac.stanford.edu',
      url='https://xrootd.org',
      download_url='https://github.com/xrootd/xrootd/archive/v%s.tar.gz' % version,
      keywords=['XRootD', 'network filesystem'],
      license='LGPL-3.0-or-later',
      long_description=open(srcdir + '/README.md').read(),
      long_description_content_type='text/plain',
      packages = ['XRootD', 'XRootD.client', 'pyxrootd'],
      package_data = {
        'XRootD.client': ['py.typed'],
      },
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
          "Operating System :: MacOS",
          "Operating System :: POSIX :: Linux",
          "Operating System :: Unix",
          "Programming Language :: C++",
          "Programming Language :: Python",
      ]
     )
