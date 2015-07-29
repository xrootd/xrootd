from distutils.core import setup, Extension
from distutils import sysconfig
from os import getenv, walk, path, path, getcwd, chdir
import subprocess

# Remove the "-Wstrict-prototypes" compiler option, which isn't valid for C++.
cfg_vars = sysconfig.get_config_vars()
opt = cfg_vars["OPT"]
cfg_vars["OPT"] = " ".join( flag for flag in opt.split() if flag != '-Wstrict-prototypes' )


sources = list()
depends = list()

for dirname, dirnames, filenames in walk('src'):
  for filename in filenames:
    if filename.endswith('.cc'):
      sources.append(path.join(dirname, filename))
    elif filename.endswith('.hh'):
      depends.append(path.join(dirname, filename))

xrdlibdir = getenv( 'XRD_LIBDIR' ) or '/usr/lib'
xrdincdir = getenv( 'XRD_INCDIR' ) or '/usr/include/xrootd'

# Get package version
with open ('VERSION_INFO') as verfile:
    version = verfile.read().strip()

print 'XRootD library dir: ', xrdlibdir
print 'XRootD include dir: ', xrdincdir
print 'Version:            ', version

setup( name             = 'pyxrootd',
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
