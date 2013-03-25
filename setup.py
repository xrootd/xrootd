from distutils.core import setup, Extension
from distutils import sysconfig
from os import getenv
from subprocess import call
import os

xrdlibdir = getenv( 'XRD_LIBDIR' ) or '/usr/lib'
xrdincdir = getenv( 'XRD_INCDIR' ) or '/usr/include/xrootd'

print ('XRootD library dir:', xrdlibdir)
print ('XRootD include dir:', xrdincdir)

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
               sources      = ['src/PyXRootDModule.cc', 'src/PyXRootDFile.cc',
                               'src/PyXRootDFileSystem.cc', 'src/PyXRootDURL.cc',
                               'src/Utils.cc'],
               depends      = ['src/PyXRootD.hh', 'src/PyXRootDClient.hh', 
                               'src/PyXRootDURL.hh', 'src/Utils.hh', 
                               'src/AsyncResponseHandler.hh',
                               'src/PyXRootDDocumentation.hh'],
               libraries    = ['XrdCl', 'XrdUtils', 'dl'],
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
call(["make", "-C", "docs", "html"])
