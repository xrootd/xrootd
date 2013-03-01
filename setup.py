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
               sources      = ['src/XrdClBind.cc', 'src/XrdClFileSystemBind.cc'],
               depends      = ['src/AsyncResponseHandler.hh', 'src/ClientType.hh'
                               'src/HostInfoType.hh', 'src/StatInfoType.hh',
                               'src/URLType.hh', 'src/XrdClBindUtils.hh'],
               libraries    = ['XrdCl', 'XrdUtils', 'dl'],
               extra_compile_args = ['-g', 
                                     '-Wno-deprecated-writable-strings',
                                     '-Wno-deprecated',
                                     '-Wno-shorten-64-to-32', 
                                     '-Wno-write-strings'],
              extra_link_args     = ['-Wl,--no-undefined'],
               include_dirs = [xrdincdir],
               library_dirs = [xrdlibdir]
               )
           ]
       )
