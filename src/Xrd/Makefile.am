#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

## Note: XrdObject.hh XrdObject.icc seem not to be used anywhere
EXTRA_DIST = XrdObject.hh XrdObject.icc

xrootdxrdincdir = $(includedir)/xrootd/Xrd

xrootdxrdinc_HEADERS = XrdLink.hh XrdBuffer.hh XrdScheduler.hh XrdProtocol.hh \
                       XrdObject.hh XrdJob.hh XrdLinkMatch.hh XrdObject.icc

lib_LTLIBRARIES = libXrd.la

INCLUDES = -I$(top_srcdir)/src

libXrd_la_SOURCES = \
        XrdBuffer.cc XrdBuffer.hh     \
        XrdConfig.cc XrdConfig.hh     \
        XrdInet.cc XrdInet.hh       \
        XrdInfo.cc XrdInfo.hh       \
	XrdLinkMatch.cc XrdLinkMatch.hh \
        XrdLink.cc XrdLink.hh       \
        XrdMain.cc        \
        XrdPoll.cc XrdPollDev.hh XrdPollPoll.hh XrdPoll.hh XrdPollPoll.icc XrdPollDev.icc   \
        XrdProtocol.cc XrdProtocol.hh   \
        XrdScheduler.cc XrdScheduler.hh XrdJob.hh  \
        XrdStats.cc XrdStats.hh \
                    XrdTrace.hh
