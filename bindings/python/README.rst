Prerequisites (incomplete): xrootd

# Installing on OSX
Setup should succeed if:
 - xrootd is installed on your system
 - xrootd was installed via homebrew
 - you're installing the bindings package with the same version number as the
   xrootd installation (`xrootd -v`).

If you have xrootd installed and the installation still fails, do
`XRD_LIBDIR=XYZ; XRD_INCDIR=ZYX; pip install xrootd`
where XYZ and ZYX are the paths to the XRootD library and include directories on your system.

## How to find the lib and inc directories
To find the library directory, search your system for "libXrd*" files.
The include directory should contain a file named "XrdVersion.hh", so search for that.
