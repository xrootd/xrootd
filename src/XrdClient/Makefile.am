#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

noinst_LTLIBRARIES = libXrdClient.la

bin_PROGRAMS = xrdcp TestXrdClient TestXrdClient_read

xrootdclientincdir = $(includedir)/xrootd/XrdClient
xrootdclientinc_HEADERS = XrdClient.hh XrdClientAbs.hh XrdClientString.hh

INCLUDES = -I$(top_srcdir)/src

libXrdClient_la_SOURCES = \
        XrdClientAbs.cc XrdClientAbs.hh                \
        XrdClient.cc XrdClient.hh                   \
        XrdClientSock.cc XrdClientSock.hh               \
        XrdClientConn.cc XrdClientConn.hh               \
        XrdClientConnMgr.cc XrdClientConnMgr.hh XrdClientUnsolMsg.hh           \
        XrdClientDebug.cc XrdClientDebug.hh              \
        XrdClientDNS.cc XrdClientDNS.hh                \
        XrdClientInputBuffer.cc XrdClientInputBuffer.hh        \
        XrdClientLogConnection.cc XrdClientLogConnection.hh      \
        XrdClientMessage.cc XrdClientMessage.hh XrdClientProtocol.hh          \
        XrdClientPhyConnection.cc XrdClientPhyConnection.hh      \
        XrdClientReadCache.cc XrdClientReadCache.hh          \
        XrdClientUrlInfo.cc XrdClientUrlInfo.hh            \
        XrdClientUrlSet.cc XrdClientUrlSet.hh             \
        XrdClientStringMatcher.cc XrdClientStringMatcher.hh      \
        XrdClientThread.cc XrdClientThread.hh             \
        XrdClientProtocol.cc            \
        XrdClientAdmin.cc XrdClientAdmin.hh XrdClientVector.hh             \
        XrdClientEnv.cc XrdClientEnv.hh XrdClientConst.hh                \
        XrdCpMthrQueue.cc  XrdCpMthrQueue.hh             \
        XrdCpWorkLst.cc XrdCpWorkLst.hh                \
        XrdClientString.cc XrdClientString.hh             \
        XrdClientSid.cc XrdClientSid.hh
libXrdClient_la_LIBADD = $(LIBSUNMATH)

##  misdeclarations in original Makefile:
##        XrdClientXrdSock.cc             \
##        XrdClientXrdFactory.cc          \
##        XrdClientXrdDNS.cc              

clientlibs =  libXrdClient.la \
              ../XrdSec/libXrdSec.la \
              ../XrdOuc/libXrdOuc.la \
              ../XrdNet/libXrdNet.la \
              -ldl

xrdcp_SOURCES = Xrdcp.cc
xrdcp_LDADD = $(clientlibs)

TestXrdClient_SOURCES = XrdClientAdmin_c.cc XrdClientAdmin_c.hh \
                        TestXrdClient.cc
TestXrdClient_LDADD =  $(clientlibs)

TestXrdClient_read_SOURCES = TestXrdClient_read.cc
TestXrdClient_read_LDADD =  $(clientlibs)



if ENABLE_PERLINT
XrdClientAdmin_c_wrap.cc:
	cp XrdClientAdmin_c_wrap.c XrdClientAdmin_c_wrap.cc

lib_LTLIBRARIES = libXrdClientAdmin.la
libXrdClientAdmin_la_SOURCES = XrdClientAdmin_c.cc \
                               XrdClientAdmin_c_wrap.cc

libXrdClientAdmin_la_CXXFLAGS = $(PERL_CCOPTS)
endif

