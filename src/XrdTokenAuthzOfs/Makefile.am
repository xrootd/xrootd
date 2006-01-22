#######################################################################
## Makefile.am for token authorization module for xrootd
##
##
## Author: Andreas Joachim Peters <andreas.joachim.peters@cern.ch>
##         Derek Feichtinger <derek.feichtinger@cern.ch>
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

lib_LTLIBRARIES = libXrdTokenAuthzOfs.la
bin_PROGRAMS = xrm

INCLUDES = -I$(XROOTD_SRCDIR)/src $(AXMLR_INC) $(SENV_INC) $(TKAUTHZ_INC)

###############################################
# sources of the original XrdOfs implementation
OFSSOURCES = \
        XrdOfsConfig.cc \
        XrdOfsEvs.cc \
        XrdOfsHandle.cc \
        XrdOfs.cc

# Note: Because of the layout of the current code, two
#       replacements are necessary in XrdOfs.cc
#       1) read in the new icc file to define the global XrdOfsFS
#       2) the XrdSfsGetFileSystem via which the main executable
#          calls the dynamic module must exist only once
$(OFSSOURCES) :
	cp $(XROOTD_SRCDIR)/src/XrdOfs/$@ .
	sed -e 's/XrdOfs\/XrdOfs.icc/XrdTokenAuthzOfs.icc/' \
            -e 's/XrdOfs\/XrdOfs.hh/XrdTokenAuthzOfs.hh/' \
            -e 's/XrdSfsGetFileSystem/XrdSfsGetFileSystemINACTIVE/' \
            -e 's/extern *XrdOfs *XrdOfsFS/extern XrdTokenAuthzOfs XrdOfsFS/' $@ > /tmp/gaga
	mv /tmp/gaga $@

###############################################

libXrdTokenAuthzOfs_la_SOURCES = \
        XrdTokenAuthzOfs.cc XrdTokenAuthzOfs.hh \
        XrdTokenAuthzOfs.icc
nodist_libXrdTokenAuthzOfs_la_SOURCES = $(OFSSOURCES)

xrm_SOURCES = xrm.cc


if XROOTD_AUTOBUILD
libXrdTokenAuthzOfs_la_LIBADD = $(XROOTD_SRCDIR)/src/XrdAcc/libXrdAcc.la \
                         $(XROOTD_SRCDIR)/src/XrdOss/libXrdOss.la \
                         $(XROOTD_SRCDIR)/src/XrdOdc/libXrdOdc.la \
                         $(XROOTD_SRCDIR)/src/XrdXr/libXrdXr.la \
                         $(TKAUTHZ_LIB) -lTokenAuthz 

xrm_LDADD = $(XROOTD_SRCDIR)/src/XrdClient/libXrdClient.la \
            $(XROOTD_SRCDIR)/src/XrdSec/libXrdSec.la \
            $(XROOTD_SRCDIR)/src/XrdOuc/libXrdOuc.la \
            $(XROOTD_SRCDIR)/src/XrdNet/libXrdNet.la \
            $(XROOTD_SRCDIR)/src/XrdSys/libXrdSys.la \
            -ldl
else
libXrdTokenAuthzOfs_la_LIBADD = $(XROOTD_LIBDIR)/libXrdAcc.a \
                         $(XROOTD_LIBDIR)/libXrdOss.a \
                         $(XROOTD_LIBDIR)/libXrdOdc.a \
                         $(XROOTD_LIBDIR)/libXrdXr.a \
                         $(TKAUTHZ_LIB) -lTokenAuthz 

xrm_LDADD = -L$(XROOTD_LIBDIR) -lXrdClient \
            -lXrdSec \
            $(XROOTD_LIBDIR)/libXrdOuc.a \
            $(XROOTD_LIBDIR)/libXrdNet.a \
            $(XROOTD_LIBDIR)/libXrdSys.a \
            -ldl
endif



CLEANFILES = $(OFSSOURCES)

## for now we still distribute the separate configure.ac to be used with the old build
EXTRA_DIST = configure.ac acinclude.m4

