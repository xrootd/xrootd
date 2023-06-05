from setuptools import setup
from setuptools.command.install import install
from setuptools.command.sdist import sdist
from wheel.bdist_wheel import bdist_wheel

# shutil.which was added in Python 3.3
# c.f. https://docs.python.org/3/library/shutil.html#shutil.which
try:
    from shutil import which
except ImportError:
    from distutils.spawn import find_executable as which

import subprocess
import sys

def get_version():
    try:
        version = open('VERSION').read().strip()
    except:
        from datetime import date
        version = 'v5.6-rc' + date.today().strftime("%Y%m%d")

    return version

def binary_exists(name):
    """Check whether `name` is on PATH."""
    return which(name) is not None

def check_cmake3(path):
    args = (path, "--version")
    popen = subprocess.Popen(args, stdout=subprocess.PIPE)
    popen.wait()
    output = popen.stdout.read().decode("utf-8") 
    prefix_len = len( "cmake version " )
    version = output[prefix_len:].split( '.' )
    return int( version[0] ) >= 3

def cmake_exists():
    """Check whether CMAKE is on PATH."""
    path = which('cmake')
    if path is not None:
        if check_cmake3(path): return True, path
    path = which('cmake3')
    return path is not None, path

def is_rhel7():
    """check if we are running on rhel7 platform"""
    try:
      f = open( '/etc/redhat-release', "r" )
      txt = f.read().split()
      i = txt.index( 'release' ) + 1
      return txt[i][0] == '7'
    except IOError:
      return False
    except ValueError:
      return False

def has_devtoolset():
    """check if devtoolset-7 is installed"""
    import subprocess
    args = ( "/usr/bin/rpm", "-q", "devtoolset-7-gcc-c++" )
    popen = subprocess.Popen(args, stdout=subprocess.PIPE)
    rc = popen.wait()
    return rc == 0

def has_cxx14():
    """check if C++ compiler supports C++14"""
    import subprocess
    popen = subprocess.Popen("./has_c++14.sh", stdout=subprocess.PIPE)
    rc = popen.wait()
    return rc == 0

# def python_dependency_name( py_version_short, py_version_nodot ):
#     """ find the name of python dependency """
#     # this is the path to default python
#     path = which( 'python' )
#     from os.path import realpath
#     # this is the real default python after resolving symlinks
#     real = realpath(path)
#     index = real.find( 'python' ) + len( 'python' )
#     # this is the version of default python
#     defaultpy = real[index:]
#     if defaultpy != py_version_short:
#       return 'python' + py_version_nodot
#     return 'python'

class CustomInstall(install):
    def run(self): 

        py_version_short = self.config_vars['py_version_short']
        py_version_nodot = self.config_vars['py_version_nodot']

        cmake_bin, cmake_path = cmake_exists()
        make_bin    = binary_exists( 'make' )
        comp_bin    = binary_exists( 'c++' ) or binary_exists( 'g++' ) or binary_exists( 'clang' )

        import pkgconfig
        zlib_dev     = pkgconfig.exists( 'zlib' )
        openssl_dev  = pkgconfig.exists( 'openssl' )
        uuid_dev     = pkgconfig.exists( 'uuid' )

        if is_rhel7():
          if has_cxx14():
            devtoolset7 = True # we only care about devtoolset7 if the compiler does not support C++14
            need_devtoolset = "false"
          else:
            devtoolset7 = has_devtoolset()
            need_devtoolset = "true"
        else:
          devtoolset7 = True # we only care about devtoolset7 on rhel7
          need_devtoolset = "false"
        
        pyname = None
        if py_version_nodot[0] == '3':
            python_dev = pkgconfig.exists( 'python3' ) or pkgconfig.exists( 'python' + py_version_nodot );
            pyname = 'python3'
        else:
            python_dev = pkgconfig.exists( 'python' );
            pyname = 'python'

        missing_dep = not ( cmake_bin and make_bin and comp_bin and zlib_dev and openssl_dev and python_dev and uuid_dev and devtoolset7 )

        if missing_dep:
          print( 'Some dependencies are missing:')
          if not cmake_bin:    print('\tcmake (version 3) is missing!')
          if not make_bin:     print('\tmake is missing!')
          if not comp_bin:     print('\tC++ compiler is missing (g++, c++, clang, etc.)!')
          if not zlib_dev:     print('\tzlib development package is missing!')
          if not openssl_dev:  print('\topenssl development package is missing!')
          if not python_dev:   print('\t{} development package is missing!'.format(pyname) )
          if not uuid_dev:     print('\tuuid development package is missing')
          if not devtoolset7:  print('\tdevtoolset-7-gcc-c++ package is missing')
          raise Exception( 'Dependencies missing!' )

        useropt = ''
        command = ['./install.sh']
        if self.user:
            prefix = self.install_usersite
            useropt = '--user'
        else:
            prefix = self.install_platlib
        command.append(prefix)
        command.append( py_version_short )
        command.append( useropt )
        command.append( cmake_path )
        command.append( need_devtoolset )
        command.append( sys.executable )
        rc = subprocess.call(command)
        if rc:
          raise Exception( 'Install step failed!' )


class CustomDist(sdist):
    def write_version_to_file(self):

        version = get_version()
        with open('bindings/python/VERSION', 'w') as vi:
            vi.write(version)
    
    def run(self):
        self.write_version_to_file()
        sdist.run(self)


class CustomWheelGen(bdist_wheel):
    # Do not generate wheel
    def run(self):
        pass

version = get_version()
setup_requires=[ 'pkgconfig' ]

setup( 
    name             = 'xrootd',
    version          = version,
    author           = 'XRootD Developers',
    author_email     = 'xrootd-dev@slac.stanford.edu',
    url              = 'http://xrootd.org',
    license          = 'LGPLv3+',
    description      = "XRootD with Python bindings",
    long_description = "XRootD with Python bindings",
    setup_requires   = setup_requires,
    cmdclass         = {
        'install':     CustomInstall,
        'sdist':       CustomDist,
        'bdist_wheel': CustomWheelGen
    }
)
