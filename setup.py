from distutils.core import setup, Extension
from os import getenv

xrdlibdir = getenv( 'XRD_LIBDIR' ) or '/usr/lib'
xrdincdir = getenv( 'XRD_INCDIR' ) or '/usr/include/xrootd'

print 'XRootD library dir:', xrdlibdir
print 'XRootD include dir:', xrdincdir

setup( name             = 'pyxrootd',
       version          = '0.1',
       author           = 'Justin Salmon',
       author_email     = 'jsalmon@cern.ch',
       url              = 'http://xrootd.org',
       license          = 'LGPL',
       description      = "XRootD Python bindings",
       long_description = "XRootD Python bindings",
       packages         = ['XRootD'],
       package_dir      = {'XRootD' : 'src'},
       ext_modules      = [
           Extension(
               'XRootD.client',
               sources      = ['src/XrdClBind.cc'],
               depends      = ['src/ClientType.hh', 'src/XRootDStatusType.hh',
                               'src/URLType'],
               libraries    = ['XrdCl', 'XrdUtils', 'dl'],
               extra_compile_args = ['-g'],
               include_dirs = [xrdincdir],
               library_dirs = [xrdlibdir]
               )
           ]
       )
