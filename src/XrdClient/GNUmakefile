#              $Id$

#-----------------------------------------------------------------------------#
#                            E n v i r o n m e n t                            #
#-----------------------------------------------------------------------------#
 
include ../GNUmake.env


BINLIBS = -L$(LIBDIR) -lXrdClient -lXrdNet -lXrdOuc -lXrdSec

LIBDEPS = $(LIBDIR)/libXrdNet.a

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
	XrdClientAbs.cc	                \
	XrdClient.cc		        \
	XrdClientSock.cc	        \
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

OBJECTB = $(OBJDIR)/TestXrdClient.o
OBJECTC = $(OBJDIR)/Xrdcp.o

OBJECTPERLMOD = $(OBJDIR)/XrdClientAdmin_c.o            \
                $(OBJDIR)/XrdClientAdmin_c_wrap.o

LIBRARY = $(LIBDIR)/libXrdClient.a

TESTBIN = $(BINDIR)/TestXrdClient
XRDCP = $(BINDIR)/xrdcp
PERLMOD = $(LIBDIR)/XrdClientAdmin.so

# this is how to take perl development path automatically
PERLDEVELPATH := $(dir \
                   $(firstword \
                     $(wildcard \
                      $(addsuffix /CORE/, $(shell perl5.8 -e 'print "@INC";')))))

# flags to be used when compiling swig code
PERLCCFLAGS := $(shell perl5.8 -e 'use Config; print $Config{ccflags};')

PERLINC = -I$(PERLDEVELPATH)

TARGETS = $(LIBRARY) $(TESTBIN) $(XRDCP)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                           -
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdClient done.

perlmod: $(TARGETS) $(PERLMOD)
	@echo Make perlmodule done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(LIBRARY): $(OBJECTS)
	@echo Creating archive $(LIBRARY) 
	@rm -f $(LIBRARY);
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	$(CC) -xar -o $(LIBRARY) $(OBJECTS); \
else \
	ar -rc $(LIBRARY) $(OBJECTS); \
fi

$(TESTBIN): $(OBJECTS) $(OBJECTB) $(LIBDEPS)
	@echo Creating executable $(TESTBIN) 
	@$(LD) $(OBJECTB) $(BINLIBS) $(LIBS) -o $(TESTBIN)

$(XRDCP): $(OBJECTS) $(OBJECTC) $(LIBDEPS)
	@echo Creating executable $(XRDCP) 
	@$(LD) $(OBJECTC) $(BINLIBS) $(LIBS) -o $(XRDCP)

$(PERLMOD): $(OBJECTS) $(OBJECTC) $(OBJECTPERLMOD) $(LIBDEPS)
	@echo Creating perl module library $(PERLMOD) 
	@$(CC) $(OBJECTC) $(OBJECTPERLMOD) $(BINLIBS) $(LDSO) $(PERLCCFLAGS) $(LIBS) -o $(PERLMOD)

$(OBJDIR)/XrdClientAdmin_c_wrap.o:        XrdClientAdmin_c_wrap.c
	@echo Compiling XrdClientAdmin_c_wrap.c
	$(CC) -g -c $(CFLAGS) $(INCLUDE) $(PERLINC) $(PERLCCFLAGS) -o $(OBJDIR)/XrdClientAdmin_c_wrap.o XrdClientAdmin_c_wrap.c

$(OBJDIR)/XrdClientAdmin_c.o:        XrdClientAdmin_c.cc   XrdClientAdmin_c.hh
	@echo Compiling XrdClientAdmin_c.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientAdmin_c.o XrdClientAdmin_c.cc

$(OBJDIR)/XrdClientString.o:        XrdClientString.cc     XrdClientString.hh
	@echo Compiling XrdClientString.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientString.o XrdClientString.cc

$(OBJDIR)/XrdClientAbs.o:        XrdClientAbs.cc              XrdClientAbs.hh\
                                 XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientString.hh
	@echo Compiling XrdClientAbs.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientAbs.o XrdClientAbs.cc

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
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClient.o XrdClient.cc


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
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientAdmin.o XrdClientAdmin.cc

$(OBJDIR)/XrdClientSock.o:       XrdClientSock.cc             XrdClientSock.hh\
                                 XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientSock.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientSock.o XrdClientSock.cc

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
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientConn.o XrdClientConn.cc

$(OBJDIR)/XrdClientConnMgr.o:    XrdClientConnMgr.cc          XrdClientConnMgr.hh\
                                 XrdClientMessage.cc          XrdClientMessage.hh\
                                 XrdClientPhyConnection.cc    XrdClientPhyConnection.hh\
                                 XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                 XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientConnMgr.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientConnMgr.o XrdClientConnMgr.cc

$(OBJDIR)/XrdClientDebug.o:      XrdClientDebug.cc            XrdClientDebug.hh\
                                 XrdClientConst.hh\
                                 XrdClientEnv.cc              XrdClientEnv.hh\
                                 XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientDebug.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientDebug.o XrdClientDebug.cc

$(OBJDIR)/XrdClientInputBuffer.o:      XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientString.hh\
                                       XrdClientVector.hh
	@echo Compiling XrdClientInputBuffer.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientInputBuffer.o XrdClientInputBuffer.cc

$(OBJDIR)/XrdClientLogConnection.o:    XrdClientLogConnection.cc    XrdClientLogConnection.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientString.hh\
                                       XrdClientVector.hh
	@echo Compiling XrdClientLogConnection.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientLogConnection.o XrdClientLogConnection.cc

$(OBJDIR)/XrdClientMessage.o:          XrdClientMessage.cc          XrdClientMessage.hh\
                                       XrdClientPhyConnection.cc    XrdClientPhyConnection.hh\
                                       XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientString.hh\
                                       XrdClientVector.hh
	@echo Compiling XrdClientMessage.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientMessage.o XrdClientMessage.cc

$(OBJDIR)/XrdClientPhyConnection.o:    XrdClientPhyConnection.cc    XrdClientPhyConnection.hh\
                                       XrdClientInputBuffer.cc      XrdClientInputBuffer.hh\
                                       XrdClientSock.cc             XrdClientSock.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientReadCache.cc\
			 	       XrdClientReadCache.hh\
                                       XrdClientEnv.cc              XrdClientEnv.hh\
                                       XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientPhyConnection.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientPhyConnection.o XrdClientPhyConnection.cc

$(OBJDIR)/XrdClientReadCache.o:        XrdClientReadCache.cc        XrdClientReadCache.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh\
                                       XrdClientEnv.cc              XrdClientEnv.hh\
                                       XrdClientString.hh           XrdClientVector.hh
	@echo Compiling XrdClientReadCache.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientReadCache.o XrdClientReadCache.cc

$(OBJDIR)/XrdClientUrlInfo.o:          XrdClientUrlInfo.cc          XrdClientUrlInfo.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientString.hh\
                                       XrdClientVector.hh
	@echo Compiling XrdClientUrlInfo.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientUrlInfo.o XrdClientUrlInfo.cc

$(OBJDIR)/XrdClientUrlSet.o:           XrdClientUrlSet.cc           XrdClientUrlSet.hh\
                                       XrdClientUrlInfo.cc          XrdClientUrlInfo.hh\
                                       XrdClientDebug.cc            XrdClientDebug.hh\
                                       XrdClientConst.hh            XrdClientString.hh\
                                       XrdClientVector.hh
	@echo Compiling XrdClientUrlSet.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientUrlSet.o XrdClientUrlSet.cc

$(OBJDIR)/XrdClientProtocol.o:        XrdClientProtocol.cc        XrdClientProtocol.hh\
                                      XrdClientDebug.cc           XrdClientDebug.hh\
                                      XrdClientConst.hh           XrdClientString.hh\
                                      XrdClientVector.hh
	@echo Compiling XrdClientProtocol.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientProtocol.o XrdClientProtocol.cc

$(OBJDIR)/TestXrdClient.o:       TestXrdClient.cc\
                                 XrdClientConst.hh                XrdClientString.hh\
                                 XrdClientVector.hh
	@echo Compiling TestXrdClient.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/TestXrdClient.o TestXrdClient.cc

$(OBJDIR)/XrdCpMthrQueue.o:      XrdCpMthrQueue.cc\
                                 XrdCpMthrQueue.hh                XrdClientString.hh\
                                 XrdClientVector.hh
	@echo Compiling XrdMthrQueue.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdCpMthrQueue.o XrdCpMthrQueue.cc

$(OBJDIR)/Xrdcp.o:               Xrdcp.cc\
                                 XrdCpMthrQueue.hh\
				 XrdCpWorkLst.hh\
                                 XrdClientString.hh\
                                 XrdClientVector.hh
	@echo Compiling Xrdcp.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/Xrdcp.o Xrdcp.cc

$(OBJDIR)/XrdClientEnv.o:        XrdClientEnv.cc                  XrdClientEnv.hh\
                                 XrdClientString.hh               XrdClientVector.hh
	@echo Compiling XrdClientEnv.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientEnv.o XrdClientEnv.cc

$(OBJDIR)/XrdClientStringMatcher.o:      XrdClientStringMatcher.cc\
                                         XrdClientStringMatcher.hh\
                                         XrdClientString.hh
	@echo Compiling XrdClientStringMatcher.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientStringMatcher.o XrdClientStringMatcher.cc

$(OBJDIR)/XrdCpWorkLst.o:          XrdCpWorkLst.cc\
                                   XrdCpWorkLst.hh\
                                   XrdClientString.hh\
                                   XrdClientVector.hh
	@echo Compiling XrdCpWorkLst.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdCpWorkLst.o XrdCpWorkLst.cc
