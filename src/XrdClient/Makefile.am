#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################


AM_CPPFLAGS = -I$(top_srcdir)/src $(JAVA_INCDIR) 

#############################################################################
## extra directory definitions
javaclassdir = $(JAVA_CLASSPATH)
xrootdclientincdir = $(includedir)/xrootd/XrdClient


#############################################################################
## deliverables
bin_PROGRAMS = xrdcp TestXrdClient TestXrdClient_read xrd

## old automake does not know += operator, so we have to do this
## very awkwardly using the EXTRA_ definition
if ENABLE_JAVAINT
  BUILDJAVA = libXrdClientAdminJNI.la
endif

if ENABLE_PERLINT
  BUILDPERL = libXrdClientAdmin.la
endif
lib_LTLIBRARIES = libXrdClient.la $(BUILDPERL) $(BUILDJAVA)

xrootdclientinc_HEADERS = XrdClient.hh \
               XrdClientVector.hh XrdClientAdmin.hh \
               XrdClientAbs.hh XrdClientUnsolMsg.hh XrdClientMessage.hh \
               XrdClientProtocol.hh XrdClientSock.hh XrdClientUrlInfo.hh \
               XrdClientConn.hh XrdClientConst.hh XrdClientReadCache.hh \
               XrdClientInputBuffer.hh XrdClientEnv.hh XrdClientThread.hh

#############################################################################
# Build rules

libXrdClient_la_SOURCES = \
        XrdClientAbs.cc XrdClientAbs.hh                \
        XrdClient.cc XrdClient.hh                   \
        XrdClientSock.cc XrdClientSock.hh               \
        XrdClientConn.cc XrdClientConn.hh               \
        XrdClientConnMgr.cc XrdClientConnMgr.hh XrdClientUnsolMsg.hh           \
        XrdClientDebug.cc XrdClientDebug.hh              \
        XrdClientInputBuffer.cc XrdClientInputBuffer.hh        \
        XrdClientLogConnection.cc XrdClientLogConnection.hh      \
        XrdClientMessage.cc XrdClientMessage.hh XrdClientProtocol.hh          \
        XrdClientPhyConnection.cc XrdClientPhyConnection.hh      \
        XrdClientReadCache.cc XrdClientReadCache.hh          \
        XrdClientUrlInfo.cc XrdClientUrlInfo.hh            \
        XrdClientUrlSet.cc XrdClientUrlSet.hh             \
        XrdClientThread.cc XrdClientThread.hh             \
        XrdClientProtocol.cc            \
        XrdClientAdmin.cc XrdClientAdmin.hh XrdClientVector.hh             \
        XrdClientEnv.cc XrdClientEnv.hh XrdClientConst.hh                \
        XrdCpMthrQueue.cc  XrdCpMthrQueue.hh             \
        XrdCpWorkLst.cc XrdCpWorkLst.hh                \
        XrdClientSid.cc XrdClientSid.hh
libXrdClient_la_LIBADD = $(LIBSUNMATH)

clientlibs =  libXrdClient.la \
              ../XrdOuc/libXrdOuc.la \
              ../XrdNet/libXrdNet.la \
              ../XrdSys/libXrdSys.la \
              -ldl

xrdcp_SOURCES = Xrdcp.cc
xrdcp_LDADD = $(clientlibs)

xrd_SOURCES = XrdCommandLine.cc
xrd_LDADD = $(clientlibs)

TestXrdClient_SOURCES = XrdClientAdmin_c.cc XrdClientAdmin_c.hh \
                        TestXrdClient.cc
TestXrdClient_LDADD =  $(clientlibs)

TestXrdClient_read_SOURCES = TestXrdClient_read.cc
TestXrdClient_read_LDADD =  $(clientlibs)

if ENABLE_JAVAINT
libXrdClientAdminJNI_la_SOURCES = XrdClientAdminJNI.cc
libXrdClientAdminJNI_la_LIBADD = $(clientlibs)                                                        

all-local: XrdClientAdminJNI.class
XrdClientAdminJNI.class: XrdClientAdminJNI.java
	$(JAVAC) -d ./ $(srcdir)/XrdClientAdminJNI.java
install-exec-local:  XrdClientAdminJNI.class
	$(INSTALL) -d $(javaclassdir)/xrootdadmin
	$(INSTALL) -m 644 xrootdadmin/XrdClientAdminJNI.class $(javaclassdir)/xrootdadmin/XrdClientAdminJNI.class
clean-local:
	rm -f xrootdadmin/XrdClientAdminJNI.class
uninstall-local:
	rm -f $(javaclassdir)/xrootdadmin/XrdClientAdminJNI.class
endif

if ENABLE_PERLINT
XrdClientAdmin_c_wrap.cc:
	cp XrdClientAdmin_c_wrap.c XrdClientAdmin_c_wrap.cc

libXrdClientAdmin_la_SOURCES = XrdClientAdmin_c.cc \
                               XrdClientAdmin_c_wrap.cc

libXrdClientAdmin_la_CXXFLAGS = $(PERL_CCOPTS)
endif


