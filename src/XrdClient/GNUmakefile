#              $Id$

#-----------------------------------------------------------------------------#
#                            E n v i r o n m e n t                            #
#-----------------------------------------------------------------------------#
include ../GNUmake.env

BINLIBS = -L$(LIBDIR) -lXrdClient -lXrdClientXrd -lXrdNet -lXrdOuc -lXrdSec

LIBDEPS = $(LIBDIR)/libXrdNet.a

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
SOURCES = \
	XrdClientAbs.cc	                \
	XrdClient.cc		        \
	XrdClientDNS.cc	                \
	XrdClientXrdDNS.cc	        \
	XrdClientSock.cc	        \
	XrdClientXrdSock.cc	        \
	XrdClientXrdFactory.cc	        \
	XrdClientConn.cc		\
	XrdClientConnMgr.cc		\
	XrdClientDebug.cc		\
	XrdClientInputBuffer.cc	        \
	XrdClientLogConnection.cc	\
	XrdClientMessage.cc		\
	XrdClientPhyConnection.cc	\
	XrdClientReadCache.cc		\
	XrdClientUrlInfo.cc		\
	XrdClientUrlSet.cc		\
	XrdClientStringMatcher.cc       \
	XrdClientProtocol.cc            \
	XrdClientAdmin.cc		\
	XrdCpMthrQueue.cc		\
	XrdCpWorkLst.cc                 \
	XrdClientString.cc              \
        XrdClientAdmin_c.cc             \
        XrdClientAdmin_c_wrap.c

  
OBJECTS = \
	$(OBJDIR)/XrdClientAbs.o                \
	$(OBJDIR)/XrdClient.o                   \
	$(OBJDIR)/XrdClientSock.o               \
	$(OBJDIR)/XrdClientDNS.o                \
	$(OBJDIR)/XrdClientConn.o		\
	$(OBJDIR)/XrdClientConnMgr.o		\
	$(OBJDIR)/XrdClientDebug.o		\
	$(OBJDIR)/XrdClientInputBuffer.o	\
	$(OBJDIR)/XrdClientLogConnection.o	\
	$(OBJDIR)/XrdClientMessage.o		\
	$(OBJDIR)/XrdClientPhyConnection.o	\
	$(OBJDIR)/XrdClientReadCache.o          \
	$(OBJDIR)/XrdClientUrlInfo.o		\
	$(OBJDIR)/XrdClientUrlSet.o		\
	$(OBJDIR)/XrdClientProtocol.o           \
	$(OBJDIR)/XrdClientStringMatcher.o      \
	$(OBJDIR)/XrdClientAdmin.o              \
	$(OBJDIR)/XrdClientEnv.o		\
	$(OBJDIR)/XrdCpMthrQueue.o		\
	$(OBJDIR)/XrdCpWorkLst.o		\
	$(OBJDIR)/XrdClientString.o              

OBJECTB = $(OBJDIR)/TestXrdClient.o $(OBJDIR)/XrdClientAdmin_c.o
OBJECTC = $(OBJDIR)/Xrdcp.o

OBJECTPERLMOD = $(OBJDIR)/XrdClientAdmin_c.o            \
                $(OBJDIR)/XrdClientAdmin_c_wrap.o

OBJECTD = \
	$(OBJDIR)/XrdClientXrdDNS.o	        \
	$(OBJDIR)/XrdClientXrdSock.o	        \
	$(OBJDIR)/XrdClientXrdFactory.o		\
	$(OBJDIR)/XrdClientThreadImp.o

LIBRARY = $(LIBDIR)/libXrdClient.a
LIBAUX = $(LIBDIR)/libXrdClientXrd.a

TESTBIN = $(BINDIR)/TestXrdClient
XRDCP = $(BINDIR)/xrdcp
PERLMOD =
ifneq ($(PERLINC),)
PERLMOD = $(LIBDIR)/XrdClientAdmin.so
endif


TARGETS = $(LIBRARY) $(LIBAUX) $(TESTBIN) $(XRDCP)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                           -
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS) perlmod
	@echo Make XrdClient done.

perlmod: $(PERLMOD)
	$(ECHO)cp *.pm $(LIBDIR)
	@echo Make perlmodule done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(LIBRARY): $(OBJECTS)
	@echo Creating archive $(LIBRARY) 
	$(ECHO)rm -f $(LIBRARY);
	$(ECHO)if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	   $(CC) -xar -o $(LIBRARY) $(OBJECTS); \
	else \
	   ar -rc $(LIBRARY) $(OBJECTS); \
	   ranlib $(LIBRARY); \
	fi

$(LIBAUX): $(OBJECTD)
	@echo Creating archive $(LIBAUX) 
	$(ECHO)rm -f $(LIBAUX);
	$(ECHO)if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	   $(CC) -xar -o $(LIBAUX) $(OBJECTD); \
	else \
	   ar -rc $(LIBAUX) $(OBJECTD); \
	   ranlib $(LIBAUX); \
	fi

$(TESTBIN): $(OBJECTS) $(OBJECTB) $(LIBDEPS)
	@echo Creating executable $(TESTBIN) 
	$(ECHO)$(LD) $(LDOP) $(OBJECTB) $(BINLIBS) $(LIBS) -o $(TESTBIN)

$(XRDCP): $(OBJECTS) $(OBJECTC) $(LIBDEPS)
	@echo Creating executable $(XRDCP) 
	$(ECHO)$(LD) $(LDOP) $(OBJECTC) $(BINLIBS) $(LIBS) -o $(XRDCP)

$(PERLMOD): $(OBJECTS) $(OBJECTC) $(OBJECTPERLMOD) $(LIBDEPS)
	@echo Creating perl module library $(PERLMOD) 
	$(ECHO)$(CC) $(OBJECTC) $(OBJECTPERLMOD) $(BINLIBS) $(LDSO) $(PERLCCFLAGS) $(LIBS) -o $(PERLMOD)

$(OBJDIR)/XrdClientAdmin_c_wrap.o:        XrdClientAdmin_c_wrap.c
	@echo Compiling XrdClientAdmin_c_wrap.c
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) $(PERLINC) $(PERLCCFLAGS) -o $(OBJDIR)/XrdClientAdmin_c_wrap.o XrdClientAdmin_c_wrap.c

$(OBJDIR)/XrdClientAdmin_c.o:        XrdClientAdmin_c.cc   XrdClientAdmin_c.hh
	@echo Compiling XrdClientAdmin_c.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) $(PERLCCFLAGS) -o $(OBJDIR)/XrdClientAdmin_c.o XrdClientAdmin_c.cc

$(OBJDIR)/XrdClientString.o:        XrdClientString.cc     XrdClientString.hh
	@echo Compiling XrdClientString.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientString.o XrdClientString.cc

$(OBJDIR)/XrdClientAbs.o:        XrdClientAbs.cc              XrdClientAbs.hh\
                                 XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientString.hh
	@echo Compiling XrdClientAbs.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientAbs.o XrdClientAbs.cc

$(OBJDIR)/XrdClient.o:           XrdClient.cc                 XrdClient.hh\
                                 XrdClientConn.cc             XrdClientConn.hh\
                                 XrdClientConnMgr.cc          XrdClientConnMgr.hh\
                                 XrdClientMessage.cc          XrdClientMessage.hh\
                                 XrdClientPhyConnection.cc    XrdClientPhyConnection.hh\
                                 XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                 XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientReadCache.cc        XrdClientReadCache.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClient.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClient.o XrdClient.cc


$(OBJDIR)/XrdClientAdmin.o:      XrdClientAdmin.cc            XrdClientAdmin.hh\
                                 XrdClientConn.cc             XrdClientConn.hh\
                                 XrdClientConnMgr.cc          XrdClientConnMgr.hh\
                                 XrdClientMessage.cc          XrdClientMessage.hh\
                                 XrdClientPhyConnection.cc    XrdClientPhyConnection.hh\
                                 XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                 XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientReadCache.cc        XrdClientReadCache.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientAdmin.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientAdmin.o XrdClientAdmin.cc

$(OBJDIR)/XrdClientSock.o:       XrdClientSock.cc             XrdClientSock.hh\
                                 XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientSock.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientSock.o XrdClientSock.cc

$(OBJDIR)/XrdClientXrdSock.o: XrdClientXrdSock.cc XrdClientXrdSock.hh XrdClientSock.hh\
                                 XrdClientDebug.cc  XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientXrdSock.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientXrdSock.o XrdClientXrdSock.cc


$(OBJDIR)/XrdClientDNS.o:       XrdClientDNS.cc             XrdClientDNS.hh\
                                 XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientDNS.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientDNS.o XrdClientDNS.cc

$(OBJDIR)/XrdClientXrdDNS.o: XrdClientXrdDNS.cc XrdClientXrdDNS.hh XrdClientDNS.hh\
                                 XrdClientDebug.cc  XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientXrdDNS.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientXrdDNS.o XrdClientXrdDNS.cc

$(OBJDIR)/XrdClientThreadImp.o: XrdClientThreadImp.cc
	@echo Compiling XrdClientThreadImp.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientThreadImp.o XrdClientThreadImp.cc

$(OBJDIR)/XrdClientXrdFactory.o: XrdClientXrdFactory.cc XrdClientXrdFactory.hh \
				 XrdClientSock.hh\
				 XrdClientMutex.hh\
				 XrdClientCond.hh\
				 XrdClientDebug.cc  XrdClientDebug.hh\
				 XrdClientConst.hh\
				 XrdClientEnv.cc              XrdClientEnv.hh\
				 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientXrdFactory.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientXrdFactory.o XrdClientXrdFactory.cc

$(OBJDIR)/XrdClientConn.o:       XrdClientConn.cc             XrdClientConn.hh\
                                 XrdClientConnMgr.cc          XrdClientConnMgr.hh\
                                 XrdClientMessage.cc          XrdClientMessage.hh\
                                 XrdClientPhyConnection.cc    XrdClientPhyConnection.hh\
                                 XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                 XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientConst.hh            XrdClientReadCache.cc\
                                 XrdClientReadCache.hh        XrdClientStringMatcher.cc\
                                 XrdClientStringMatcher.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientConn.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientConn.o XrdClientConn.cc

$(OBJDIR)/XrdClientConnMgr.o:    XrdClientConnMgr.cc          XrdClientConnMgr.hh\
                                 XrdClientMessage.cc          XrdClientMessage.hh\
                                 XrdClientPhyConnection.cc    XrdClientPhyConnection.hh\
                                 XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                 XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientConnMgr.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientConnMgr.o XrdClientConnMgr.cc

$(OBJDIR)/XrdClientDebug.o:      XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientDebug.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientDebug.o XrdClientDebug.cc

$(OBJDIR)/XrdClientInputBuffer.o:      XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientString.hh\
                                       XrdClientVector.hh
	@echo Compiling XrdClientInputBuffer.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientInputBuffer.o XrdClientInputBuffer.cc

$(OBJDIR)/XrdClientLogConnection.o:    XrdClientLogConnection.cc    XrdClientLogConnection.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientString.hh\
                                       XrdClientVector.hh
	@echo Compiling XrdClientLogConnection.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientLogConnection.o XrdClientLogConnection.cc

$(OBJDIR)/XrdClientMessage.o:          XrdClientMessage.cc          XrdClientMessage.hh\
                                       XrdClientPhyConnection.cc    XrdClientPhyConnection.hh\
                                       XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientString.hh\
                                       XrdClientVector.hh
	@echo Compiling XrdClientMessage.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientMessage.o XrdClientMessage.cc

$(OBJDIR)/XrdClientPhyConnection.o:    XrdClientPhyConnection.cc    XrdClientPhyConnection.hh\
                                       XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                       XrdClientSock.cc             XrdClientSock.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientReadCache.cc\
			 	       XrdClientReadCache.hh\
                                       XrdClientEnv.cc              XrdClientEnv.hh\
                                       XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientPhyConnection.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientPhyConnection.o XrdClientPhyConnection.cc

$(OBJDIR)/XrdClientReadCache.o:        XrdClientReadCache.cc        XrdClientReadCache.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh\
                                       XrdClientEnv.cc              XrdClientEnv.hh\
                                       XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientReadCache.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientReadCache.o XrdClientReadCache.cc

$(OBJDIR)/XrdClientUrlInfo.o:          XrdClientUrlInfo.cc          XrdClientUrlInfo.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientString.hh\
                                       XrdClientVector.hh
	@echo Compiling XrdClientUrlInfo.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientUrlInfo.o XrdClientUrlInfo.cc

$(OBJDIR)/XrdClientUrlSet.o:           XrdClientUrlSet.cc           XrdClientUrlSet.hh\
                                       XrdClientUrlInfo.cc          XrdClientUrlInfo.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientString.hh\
                                       XrdClientVector.hh
	@echo Compiling XrdClientUrlSet.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientUrlSet.o XrdClientUrlSet.cc

$(OBJDIR)/XrdClientProtocol.o:        XrdClientProtocol.cc        XrdClientProtocol.hh\
                                      XrdClientDebug.cc           XrdClientDebug.hh\
                                      XrdClientConst.hh           XrdClientString.hh\
                                      XrdClientVector.hh
	@echo Compiling XrdClientProtocol.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientProtocol.o XrdClientProtocol.cc

$(OBJDIR)/TestXrdClient.o:       TestXrdClient.cc\
                                 XrdClientConst.hh                XrdClientString.hh\
                                 XrdClientVector.hh
	@echo Compiling TestXrdClient.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/TestXrdClient.o TestXrdClient.cc

$(OBJDIR)/XrdCpMthrQueue.o:      XrdCpMthrQueue.cc\
                                 XrdCpMthrQueue.hh                XrdClientString.hh\
                                 XrdClientVector.hh
	@echo Compiling XrdMthrQueue.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdCpMthrQueue.o XrdCpMthrQueue.cc

$(OBJDIR)/Xrdcp.o:               Xrdcp.cc\
                                 XrdCpMthrQueue.hh\
				 XrdCpWorkLst.hh\
                                 XrdClientString.hh\
                                 XrdClientVector.hh
	@echo Compiling Xrdcp.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/Xrdcp.o Xrdcp.cc

$(OBJDIR)/XrdClientEnv.o:        XrdClientEnv.cc                  XrdClientEnv.hh\
                                 XrdClientString.hh               XrdClientVector.hh
	@echo Compiling XrdClientEnv.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientEnv.o XrdClientEnv.cc

$(OBJDIR)/XrdClientStringMatcher.o:      XrdClientStringMatcher.cc\
                                         XrdClientStringMatcher.hh\
                                         XrdClientString.hh
	@echo Compiling XrdClientStringMatcher.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientStringMatcher.o XrdClientStringMatcher.cc

$(OBJDIR)/XrdCpWorkLst.o:          XrdCpWorkLst.cc\
                                   XrdCpWorkLst.hh\
                                   XrdClientString.hh\
                                   XrdClientVector.hh
	@echo Compiling XrdCpWorkLst.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdCpWorkLst.o XrdCpWorkLst.cc
