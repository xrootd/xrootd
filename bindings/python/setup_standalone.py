from __future__ import print_function
from setuptools import setup
from distutils.core import Extension
from distutils import sysconfig
from os import getenv, walk, path, path, getcwd, chdir
import sys
import subprocess

# Remove the "-Wstrict-prototypes" compiler option, which isn't valid for C++.
cfg_vars = sysconfig.get_config_vars()
opt = cfg_vars["OPT"]
cfg_vars["OPT"] = " ".join( flag for flag in opt.split() if flag not in ['-Wstrict-prototypes'  ] )

cflags = cfg_vars["CFLAGS"]
cfg_vars["CFLAGS"] = " ".join( flag for flag in cflags.split() if flag not in ['-Wstrict-prototypes'  ] )

py_cflags = cfg_vars["PY_CFLAGS"]
cfg_vars["PY_CFLAGS"] = " ".join( flag for flag in py_cflags.split() if flag not in ['-Wstrict-prototypes'  ] )


sources = list()
depends = list()

for dirname, dirnames, filenames in walk('src'):
  for filename in filenames:
    if filename.endswith('.cc'):
      sources.append(path.join(dirname, filename))
    elif filename.endswith('.hh'):
      depends.append(path.join(dirname, filename))


version = subprocess.check_output(["xrootd-config", "--version"]).decode(sys.getdefaultencoding()).strip()
prefix = subprocess.check_output(["xrootd-config", "--prefix"]).decode(sys.getdefaultencoding()).strip()
print(version)
print(prefix)
xrdlibdir = path.join(prefix, "lib")
if not path.exists(xrdlibdir):
    xrdlibdir = path.join(prefix, "lib64")
xrdincdir = path.join(prefix, "include", "xrootd")

print('XRootD library dir: ', xrdlibdir)
print('XRootD include dir: ', xrdincdir)
print('Version:            ', version)

setup( name             = 'xrootd',
       version          = version,
       author           = 'XRootD Developers',
       author_email     = 'xrootd-dev@slac.stanford.edu',
       url              = 'http://xrootd.org',
       license          = 'LGPLv3+',
       description      = "XRootD Python bindings",
       long_description = "XRootD Python bindings",
       packages         = ['pyxrootd', 'XRootD', 'XRootD.client'],
       package_dir      = {'pyxrootd'     : 'src',
                           'XRootD'       : 'libs',
                           'XRootD.client': 'libs/client'},
       ext_modules      = [
           Extension(
               'pyxrootd.client',
               sources   = sources,
               depends   = depends,
               libraries = ['XrdCl', 'XrdUtils', 'dl'],
               extra_compile_args = ['-g'],
               include_dirs = [xrdincdir],
               library_dirs = [xrdlibdir]
               )
           ]
       )
