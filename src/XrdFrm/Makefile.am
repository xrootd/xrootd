#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 17.5.2008
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

#############################################################################
## extra directory definitions
xrootdfrmincdir = $(includedir)/xrootd/XrdFrm

#############################################################################
## deliverables
bin_PROGRAMS = frm_pstga frm_pstgd

INCLUDES = -I$(top_srcdir)/src

xrootdfrminc_HEADERS = XrdFrmConfig.hh  \
                       XrdFrmPstg.hh    \
                       XrdFrmPstgReq.hh \
                       XrdFrmPstgXfr.hh

#############################################################################
## Build rules

XrdFrm_SOURCES = \
                  XrdFrmConfig.cc  \
                  XrdFrmPstg.cc    \
                  XrdFrmPstgMain.cc\
                  XrdFrmPstgReq.cc \
                  XrdFrmPstgXfr.cc

XrdFrm_LDADD   = \
                  ../XrdOuc/libXrdOss.la     \
                  ../XrdNet/libXrdNet.la     \
                  ../XrdOuc/libXrdOuc.la     \
                  ../XrdSys/libXrdSys.la

XrdFrm_LDFLAGS = -static -export-dynamic

$(OBJECT_XRDPSTGA) $(BINLIBS) $(LIBM) $(LIBS)
$(OBJECT_XRDPSTGD) $(BINLIBS) $(LIBM) $(LIBS)
