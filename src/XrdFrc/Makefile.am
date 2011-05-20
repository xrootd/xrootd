#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 17.5.2008
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

#
# WARNING: In this Makefile there are a number of nonideal solutions
#          that needed to be implemented. It must get reorganized
#          For the moment, please ask derek.feichtinger@psi.ch
#          before making nontrivial changes
#


AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src $(READLINE_INCDIR)

#############################################################################
## extra directory definitions
xrootdfrcincdir = $(includedir)/xrootd/XrdFrc

#############################################################################
## deliverables

noinst_HEADERS = XrdFrcCID.hh     \
                       XrdFrcProxy.hh   \
                       XrdFrcReqAgent.hh \
                       XrdFrcReqFile.hh \
                       XrdFrcRequest.hh \
                       XrdFrcTrace.hh   \
                       XrdFrcUtils.hh   \
                       XrdFrcXAttr.hh \
                       XrdFrcXLock.hh


lib_LTLIBRARIES = libXrdFrc.la

#############################################################################
## Build rules

libXrdFrc_la_SOURCES = \
                  XrdFrcCID.cc     \
                  XrdFrcProxy.cc  \
                  XrdFrcReqAgent.cc \
                  XrdFrcReqFile.cc \
                  XrdFrcTrace.cc   \
                  XrdFrcUtils.cc

libXrdFrc_la_LDFLAGS = -static
