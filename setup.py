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
       package_dir      = {'XRootD' : 'src', 'XRootD' : 'libs'},
       ext_modules      = [
           Extension(
               'XRootD.client',
               sources      = ['src/PyXRootDModule.cc', 'src/PyXRootDFile.cc',
                               'src/PyXRootDFileSystem.cc', 'src/Utils.cc'],
               depends      = ['src/PyXRootD.hh', 'src/PyXRootDType.hh', 
                               'src/PyXRootDClient.hh', 'src/PyXRootDURL.hh', 
                               'src/Utils.hh', 'src/AsyncResponseHandler.hh'],
               libraries    = ['XrdCl', 'XrdUtils', 'dl'],
               extra_compile_args = ['-g', 
                                     '-Wno-deprecated-writable-strings',
                                     '-Wno-deprecated',
                                     '-Wno-shorten-64-to-32', 
                                     '-Wno-write-strings'],
               include_dirs = [xrdincdir],
               library_dirs = [xrdlibdir]
               )
           ]
       )
