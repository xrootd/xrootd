from distutils.core import setup, Extension
from distutils import sysconfig
from os import getenv, walk, path
from subprocess import call

xrdlibdir = getenv( 'XRD_LIBDIR' ) or '/usr/lib'
xrdincdir = getenv( 'XRD_INCDIR' ) or '/usr/include/xrootd'

print ('XRootD library dir:', xrdlibdir)
print ('XRootD include dir:', xrdincdir)

sources = list()
depends = list()

for dirname, dirnames, filenames in walk('src'):
  for filename in filenames:
    if filename.endswith('.cc'):
      sources.append(path.join(dirname, filename))
    elif filename.endswith('.hh'):
      depends.append(path.join(dirname, filename))

setup( name             = 'pyxrootd',
       version          = '0.1',
       author           = 'Justin Salmon',
       author_email     = 'jsalmon@cern.ch',
       url              = 'http://xrootd.org',
       license          = 'LGPL',
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
               extra_compile_args = ['-g', '-O0', # for debugging
                                     '-Wno-deprecated',
                                     '-Wno-shorten-64-to-32', 
                                     '-Wno-write-strings'],
               include_dirs = [xrdincdir],
               library_dirs = [xrdlibdir]
               )
           ]
       )

# Make the docs
# call(["make", "-C", "docs", "html"])
