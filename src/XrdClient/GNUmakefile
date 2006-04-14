#              $Id$

#-----------------------------------------------------------------------------#
#                            E n v i r o n m e n t                            #
#-----------------------------------------------------------------------------#
include ../GNUmake.env

BINLIBS = -L$(LIBDIR) -lXrdClient -lXrdNet -lXrdOuc -lXrdSys

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
	XrdClientThread.cc              \
	XrdClientProtocol.cc            \
	XrdClientAdmin.cc		\
	XrdCpMthrQueue.cc		\
	XrdCpWorkLst.cc                 \
        XrdClientAdmin_c.cc             \
        XrdClientAdmin_c_wrap.c		\
	XrdClientSid.cc

  
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
	$(OBJDIR)/XrdClientThread.o             \
	$(OBJDIR)/XrdClientUrlInfo.o		\
	$(OBJDIR)/XrdClientUrlSet.o		\
	$(OBJDIR)/XrdClientProtocol.o           \
	$(OBJDIR)/XrdClientAdmin.o              \
	$(OBJDIR)/XrdClientEnv.o		\
	$(OBJDIR)/XrdCpMthrQueue.o		\
	$(OBJDIR)/XrdCpWorkLst.o		\
	$(OBJDIR)/XrdClientSid.o

OBJECTB = $(OBJDIR)/TestXrdClient.o $(OBJDIR)/XrdClientAdmin_c.o
OBJECTC = $(OBJDIR)/Xrdcp.o
OBJECTD = $(OBJDIR)/TestXrdClient_read.o
OBJECT_JNI = $(OBJDIR)/XrdClientAdminJNI.o

OBJECT_CLI = $(OBJDIR)/XrdCommandLine.o
OBJECTPERLMOD = $(OBJDIR)/XrdClientAdmin_c.o            \
                $(OBJDIR)/XrdClientAdmin_c_wrap.o

LIBRARY = $(LIBDIR)/libXrdClient.a

LIBRARY_JNI =
CLASS_JNI =
ifneq ($(JAVAINC),)
LIBRARY_JNI = $(LIBDIR)/libXrdClientAdminJNI.$(TYPESHLIB)
CLASS_JNI = XrdClientAdminJNI
CLASS_JNI_TARGET = $(LIBDIR)/XrdClientAdminJNI.class
endif

TESTBIN = $(BINDIR)/TestXrdClient $(BINDIR)/TestXrdClient_read
XRDCP = $(BINDIR)/xrdcp
XRDCLI = $(BINDIR)/xrd
XRDADMIN = $(BINDIR)/xrdadmin

PERLMOD =
ifneq ($(PERLINC),)
PERLMOD = $(LIBDIR)/XrdClientAdmin.so
endif


TARGETS = $(LIBRARY) $(LIBAUX) $(TESTBIN) $(XRDCP) $(XRDCLI) $(XRDADMIN) \
	  $(LIBRARY_JNI) $(CLASS_JNI_TARGET)

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

$(LIBRARY_JNI): $(OBJECT_JNI)
	@echo Creating shared library $(LIBRARY_JNI) 
	$(ECHO)rm -f $(LIBRARY_JNI);
	$(ECHO)$(CC) $(CFLAGS) $(OBJECT_JNI) $(LDSO) $(MORELIBS) $(LIBS) -o $(LIBRARY_JNI)

$(CLASS_JNI_TARGET): $(CLASS_JNI).java
	@echo Creating JNI class file $(CLASS_JNI_TARGET)
	$(ECHO)$(JAVACBIN) -d $(LIBDIR) $(CLASS_JNI).java

$(BINDIR)/TestXrdClient_read: $(OBJECTS) $(OBJECTD) $(LIBDEPS)
	@echo Creating executable $(BINDIR)/TestXrdClient_read 
	$(ECHO)$(LD) $(LDOP) $(OBJECTD) $(BINLIBS) $(LIBS) -o $(BINDIR)/TestXrdClient_read

$(BINDIR)/TestXrdClient: $(OBJECTS) $(OBJECTB) $(LIBDEPS)
	@echo Creating executable $(BINDIR)/TestXrdClient 
	$(ECHO)$(LD) $(LDOP) $(OBJECTB) $(BINLIBS) $(LIBS) -o $(BINDIR)/TestXrdClient



$(XRDCP): $(OBJECTS) $(OBJECTC) $(LIBDEPS)
	@echo Creating executable $(XRDCP) 
	$(ECHO)$(LD) $(LDOP) $(OBJECTC) $(BINLIBS) $(LIBS) -o $(XRDCP)

$(XRDCLI): $(OBJECTS) $(OBJECT_CLI) $(LIBDEPS)
	@echo Creating executable $(XRDCLI) 
	$(ECHO)$(LD) $(LDOP) $(OBJECT_CLI) $(BINLIBS) $(LIBS) -o $(XRDCLI)

$(XRDADMIN): $(PERLMOD)
	$(ECHO)if [ "$(PERLBIN)" != "" ]; then \
		echo Creating executable $(XRDADMIN); \
		sed -e "s|#!/usr/bin/perl|#!$(PERLBIN)|" < xrdadmin > $(XRDADMIN); \
		chmod a+x $(XRDADMIN); \
	fi;

$(PERLMOD): $(OBJECTS) $(OBJECTC) $(OBJECTPERLMOD) $(LIBDEPS)
	@echo Creating perl module library $(PERLMOD) 
	$(ECHO)$(CC) $(OBJECTC) $(OBJECTPERLMOD) $(BINLIBS) $(LDSO) $(PERLCCFLAGS) $(LIBS) -o $(PERLMOD)

$(OBJDIR)/XrdClientAdmin_c_wrap.o:        XrdClientAdmin_c_wrap.c
	@echo Compiling XrdClientAdmin_c_wrap.c
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) $(PERLINC) $(PERLCCFLAGS) -o $(OBJDIR)/XrdClientAdmin_c_wrap.o XrdClientAdmin_c_wrap.c

$(OBJDIR)/XrdClientAdmin_c.o: XrdClientAdmin_c.cc ../XrdClient/XrdClientAdmin.hh \
  ../XrdClient/XrdClientAbs.hh ../XrdClient/XrdClientUnsolMsg.hh \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientSock.hh ../XrdClient/XrdClientUrlInfo.hh \
   ../XrdClient/XrdClientConn.hh \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientReadCache.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientDebug.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucLogger.hh
	@echo Compiling XrdClientAdmin_c.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) $(PERLCCFLAGS) -o $(OBJDIR)/XrdClientAdmin_c.o XrdClientAdmin_c.cc

$(OBJDIR)/XrdClientThread.o: XrdClientThread.cc ../XrdClient/XrdClientThread.hh \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh
	@echo Compiling XrdClientThread.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientThread.o XrdClientThread.cc

$(OBJDIR)/XrdClientAbs.o: XrdClientAbs.cc ../XrdClient/XrdClientAbs.hh \
  ../XrdClient/XrdClientUnsolMsg.hh ../XrdClient/XrdClientMessage.hh \
  ../XrdClient/XrdClientProtocol.hh ../XProtocol/XProtocol.hh \
  ../XProtocol/XPtypes.hh ../XrdClient/XrdClientSock.hh \
  ../XrdClient/XrdClientUrlInfo.hh  \
  ../XrdClient/XrdClientConn.hh ../XrdClient/XrdClientConst.hh \
  ../XrdClient/XrdClientReadCache.hh ../XrdClient/XrdClientInputBuffer.hh \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh \
  ../XrdOuc/XrdOucSemWait.hh ../XrdOuc/XrdOucHash.hh \
  ../XrdOuc/XrdOucHash.icc ../XrdClient/XrdClientVector.hh \
  ../XrdClient/XrdClientDebug.hh ../XrdClient/XrdClientEnv.hh \
  ../XrdOuc/XrdOucEnv.hh ../XrdOuc/XrdOucLogger.hh
	@echo Compiling XrdClientAbs.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientAbs.o XrdClientAbs.cc

$(OBJDIR)/XrdClient.o: XrdClient.cc ../XrdClient/XrdClient.hh \
  ../XrdClient/XrdClientAbs.hh ../XrdClient/XrdClientUnsolMsg.hh \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientSock.hh ../XrdClient/XrdClientUrlInfo.hh \
   ../XrdClient/XrdClientConn.hh \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientReadCache.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientDebug.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucLogger.hh ../XrdClient/XrdClientUrlSet.hh \
  ../XrdClient/XrdClientConnMgr.hh ../XrdClient/XrdClientLogConnection.hh \
  ../XrdClient/XrdClientPhyConnection.hh ../XrdClient/XrdClientThread.hh \
  ../XrdClient/XrdClientSid.hh ../XrdOuc/XrdOucRash.hh \
  ../XrdOuc/XrdOucRash.icc ../XrdSys/XrdSysPlatform.hh
	@echo Compiling XrdClient.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClient.o XrdClient.cc


$(OBJDIR)/XrdClientAdmin.o: XrdClientAdmin.cc ../XrdClient/XrdClientAdmin.hh \
  ../XrdClient/XrdClientAbs.hh ../XrdClient/XrdClientUnsolMsg.hh \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientSock.hh ../XrdClient/XrdClientUrlInfo.hh \
   ../XrdClient/XrdClientConn.hh \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientReadCache.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientDebug.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucLogger.hh ../XrdClient/XrdClientUrlSet.hh \
  ../XrdClient/XrdClientConnMgr.hh ../XrdClient/XrdClientLogConnection.hh \
  ../XrdClient/XrdClientPhyConnection.hh ../XrdClient/XrdClientThread.hh
	@echo Compiling XrdClientAdmin.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientAdmin.o XrdClientAdmin.cc

$(OBJDIR)/XrdClientAdminJNI.o: XrdClientAdminJNI.cc ../XrdClient/XrdClientAdmin.hh \
  ../XrdClient/XrdClientAbs.hh ../XrdClient/XrdClientUnsolMsg.hh \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientSock.hh ../XrdClient/XrdClientUrlInfo.hh \
   ../XrdClient/XrdClientConn.hh \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientReadCache.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientDebug.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucLogger.hh ../XrdClient/XrdClientUrlSet.hh \
  ../XrdClient/XrdClientConnMgr.hh ../XrdClient/XrdClientLogConnection.hh \
  ../XrdClient/XrdClientPhyConnection.hh ../XrdClient/XrdClientThread.hh
	@echo Compiling XrdClientAdminJNI.cc Java include path=$(JAVAINC)
	$(ECHO)$(CC) -g -c -I $(JAVAINC) -I $(JAVAINC)/linux  $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientAdminJNI.o XrdClientAdminJNI.cc
						    
$(OBJDIR)/XrdClientSock.o: XrdClientSock.cc ../XrdClient/XrdClientSock.hh \
  ../XrdClient/XrdClientUrlInfo.hh  \
  ../XrdOuc/XrdOucLogger.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdNet/XrdNetSocket.hh \
  ../XrdClient/XrdClientDebug.hh ../XrdClient/XrdClientConst.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc
	@echo Compiling XrdClientSock.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientSock.o XrdClientSock.cc

$(OBJDIR)/XrdClientConn.o: XrdClientConn.cc ../XrdClient/XrdClientDebug.hh \
  ../XrdClient/XrdClientConst.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdClient/XrdClientEnv.hh \
  ../XrdOuc/XrdOucEnv.hh ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdOuc/XrdOucLogger.hh ../XrdClient/XrdClientConnMgr.hh \
  ../XrdClient/XrdClientUnsolMsg.hh ../XrdClient/XrdClientMessage.hh \
  ../XrdClient/XrdClientProtocol.hh ../XProtocol/XProtocol.hh \
  ../XProtocol/XPtypes.hh ../XrdClient/XrdClientSock.hh \
  ../XrdClient/XrdClientUrlInfo.hh  \
  ../XrdClient/XrdClientLogConnection.hh \
  ../XrdClient/XrdClientPhyConnection.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientThread.hh \
  ../XrdClient/XrdClientConn.hh ../XrdClient/XrdClientReadCache.hh \
  ../XrdSec/XrdSecInterface.hh ../XrdSec/XrdSecEntity.hh \
  ../XrdNet/XrdNetDNS.hh  \
  ../XrdClient/XrdClientAbs.hh ../XrdClient/XrdClientSid.hh \
  ../XrdOuc/XrdOucRash.hh ../XrdOuc/XrdOucRash.icc \
  ../XrdSys/XrdSysPlatform.hh
	@echo Compiling XrdClientConn.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientConn.o XrdClientConn.cc

$(OBJDIR)/XrdClientConnMgr.o: XrdClientConnMgr.cc ../XrdClient/XrdClientConnMgr.hh \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh \
  ../XrdClient/XrdClientUnsolMsg.hh ../XrdClient/XrdClientMessage.hh \
  ../XrdClient/XrdClientProtocol.hh ../XProtocol/XProtocol.hh \
  ../XProtocol/XPtypes.hh ../XrdClient/XrdClientSock.hh \
  ../XrdClient/XrdClientUrlInfo.hh  \
  ../XrdClient/XrdClientLogConnection.hh \
  ../XrdClient/XrdClientPhyConnection.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientThread.hh \
  ../XrdClient/XrdClientDebug.hh ../XrdClient/XrdClientConst.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucLogger.hh
	@echo Compiling XrdClientConnMgr.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientConnMgr.o XrdClientConnMgr.cc

$(OBJDIR)/XrdClientDebug.o: XrdClientDebug.cc ../XrdClient/XrdClientDebug.hh \
  ../XrdClient/XrdClientConst.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdClient/XrdClientEnv.hh \
  ../XrdOuc/XrdOucEnv.hh ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdOuc/XrdOucLogger.hh
	@echo Compiling XrdClientDebug.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientDebug.o XrdClientDebug.cc

$(OBJDIR)/XrdClientInputBuffer.o: XrdClientInputBuffer.cc \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdClient/XrdClientMessage.hh \
  ../XrdClient/XrdClientProtocol.hh ../XProtocol/XProtocol.hh \
  ../XProtocol/XPtypes.hh ../XrdClient/XrdClientSock.hh \
  ../XrdClient/XrdClientUrlInfo.hh  \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh \
  ../XrdOuc/XrdOucSemWait.hh ../XrdOuc/XrdOucHash.hh \
  ../XrdOuc/XrdOucHash.icc ../XrdClient/XrdClientVector.hh \
  ../XrdClient/XrdClientDebug.hh ../XrdClient/XrdClientConst.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucLogger.hh
	@echo Compiling XrdClientInputBuffer.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientInputBuffer.o XrdClientInputBuffer.cc

$(OBJDIR)/XrdClientLogConnection.o: XrdClientLogConnection.cc \
  ../XrdClient/XrdClientLogConnection.hh \
  ../XrdClient/XrdClientUnsolMsg.hh ../XrdClient/XrdClientMessage.hh \
  ../XrdClient/XrdClientProtocol.hh ../XProtocol/XProtocol.hh \
  ../XProtocol/XPtypes.hh ../XrdClient/XrdClientSock.hh \
  ../XrdClient/XrdClientUrlInfo.hh  \
  ../XrdClient/XrdClientPhyConnection.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientThread.hh \
  ../XrdClient/XrdClientDebug.hh ../XrdClient/XrdClientConst.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucLogger.hh ../XrdClient/XrdClientSid.hh
	@echo Compiling XrdClientLogConnection.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientLogConnection.o XrdClientLogConnection.cc

$(OBJDIR)/XrdClientMessage.o: XrdClientMessage.cc ../XrdClient/XrdClientMessage.hh \
  ../XrdClient/XrdClientProtocol.hh ../XProtocol/XProtocol.hh \
  ../XProtocol/XPtypes.hh ../XrdClient/XrdClientSock.hh \
  ../XrdClient/XrdClientUrlInfo.hh  \
  ../XrdClient/XrdClientDebug.hh ../XrdClient/XrdClientConst.hh \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdOuc/XrdOucLogger.hh ../XrdClient/XrdClientPhyConnection.hh \
  ../XrdClient/XrdClientUnsolMsg.hh ../XrdClient/XrdClientInputBuffer.hh \
  ../XrdOuc/XrdOucSemWait.hh ../XrdClient/XrdClientVector.hh \
  ../XrdClient/XrdClientThread.hh
	@echo Compiling XrdClientMessage.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientMessage.o XrdClientMessage.cc

$(OBJDIR)/XrdClientPhyConnection.o: XrdClientPhyConnection.cc \
  ../XrdClient/XrdClientPhyConnection.hh ../XrdClient/XrdClientSock.hh \
  ../XrdClient/XrdClientUrlInfo.hh  \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientUnsolMsg.hh ../XrdClient/XrdClientInputBuffer.hh \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh \
  ../XrdOuc/XrdOucSemWait.hh ../XrdOuc/XrdOucHash.hh \
  ../XrdOuc/XrdOucHash.icc ../XrdClient/XrdClientVector.hh \
  ../XrdClient/XrdClientThread.hh ../XrdClient/XrdClientDebug.hh \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientEnv.hh \
  ../XrdOuc/XrdOucEnv.hh ../XrdOuc/XrdOucLogger.hh \
  ../XrdClient/XrdClientSid.hh ../XrdOuc/XrdOucRash.hh \
  ../XrdOuc/XrdOucRash.icc ../XrdSys/XrdSysPlatform.hh
	@echo Compiling XrdClientPhyConnection.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientPhyConnection.o XrdClientPhyConnection.cc

$(OBJDIR)/XrdClientReadCache.o: XrdClientReadCache.cc \
  ../XrdClient/XrdClientReadCache.hh ../XrdClient/XrdClientInputBuffer.hh \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientSock.hh ../XrdClient/XrdClientUrlInfo.hh \
  ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientConst.hh \
  ../XrdClient/XrdClientDebug.hh ../XrdClient/XrdClientEnv.hh \
  ../XrdOuc/XrdOucEnv.hh ../XrdOuc/XrdOucLogger.hh
	@echo Compiling XrdClientReadCache.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientReadCache.o XrdClientReadCache.cc

$(OBJDIR)/XrdClientUrlInfo.o: XrdClientUrlInfo.cc ../XrdClient/XrdClientUrlInfo.hh \
   ../XrdNet/XrdNetDNS.hh
	@echo Compiling XrdClientUrlInfo.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientUrlInfo.o XrdClientUrlInfo.cc

$(OBJDIR)/XrdClientUrlSet.o: XrdClientUrlSet.cc ../XrdClient/XrdClientUrlSet.hh \
  ../XrdClient/XrdClientUrlInfo.hh  \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientVector.hh \
  ../XrdClient/XrdClientDebug.hh \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdOuc/XrdOucLogger.hh
	@echo Compiling XrdClientUrlSet.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientUrlSet.o XrdClientUrlSet.cc

$(OBJDIR)/XrdClientProtocol.o: XrdClientProtocol.cc ../XProtocol/XProtocol.hh \
  ../XProtocol/XPtypes.hh ../XrdSys/XrdSysPlatform.hh
	@echo Compiling XrdClientProtocol.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientProtocol.o XrdClientProtocol.cc

$(OBJDIR)/TestXrdClient.o: TestXrdClient.cc ../XrdClient/XrdClient.hh \
  ../XrdClient/XrdClientAbs.hh ../XrdClient/XrdClientUnsolMsg.hh \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientSock.hh ../XrdClient/XrdClientUrlInfo.hh \
   ../XrdClient/XrdClientConn.hh \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientReadCache.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientAdmin_c.hh
	@echo Compiling TestXrdClient.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/TestXrdClient.o TestXrdClient.cc


$(OBJDIR)/TestXrdClient_read.o: TestXrdClient_read.cc ../XrdClient/XrdClient.hh \
  ../XrdClient/XrdClientAbs.hh ../XrdClient/XrdClientUnsolMsg.hh \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientSock.hh ../XrdClient/XrdClientUrlInfo.hh \
   ../XrdClient/XrdClientConn.hh \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientReadCache.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientAdmin_c.hh
	@echo Compiling TestXrdClient_read.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/TestXrdClient_read.o TestXrdClient_read.cc


$(OBJDIR)/XrdCpMthrQueue.o: XrdCpMthrQueue.cc ../XrdClient/XrdCpMthrQueue.hh \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh \
  ../XrdClient/XrdClientVector.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdClient/XrdClientDebug.hh ../XrdClient/XrdClientConst.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdOuc/XrdOucLogger.hh
	@echo Compiling XrdMthrQueue.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdCpMthrQueue.o XrdCpMthrQueue.cc

$(OBJDIR)/Xrdcp.o: Xrdcp.cc ../XrdClient/XrdClientUrlInfo.hh \
   ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdClient/XrdClient.hh \
  ../XrdClient/XrdClientAbs.hh ../XrdClient/XrdClientUnsolMsg.hh \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientSock.hh ../XrdClient/XrdClientConn.hh \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientReadCache.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdCpMthrQueue.hh \
  ../XrdClient/XrdClientDebug.hh ../XrdClient/XrdClientEnv.hh \
  ../XrdOuc/XrdOucEnv.hh ../XrdOuc/XrdOucLogger.hh \
  ../XrdClient/XrdCpWorkLst.hh ../XrdClient/XrdClientAdmin.hh
	@echo Compiling Xrdcp.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/Xrdcp.o Xrdcp.cc

$(OBJDIR)/XrdCommandLine.o: XrdCommandLine.cc ../XrdClient/XrdClientUrlInfo.hh \
  ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdClient/XrdClient.hh \
  ../XrdClient/XrdClientAbs.hh ../XrdClient/XrdClientUnsolMsg.hh \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientSock.hh ../XrdClient/XrdClientConn.hh \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientReadCache.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh  \
  ../XrdClient/XrdClientDebug.hh ../XrdClient/XrdClientEnv.hh \
  ../XrdOuc/XrdOucEnv.hh ../XrdOuc/XrdOucLogger.hh \
  ../XrdClient/XrdClientAdmin.hh
	@echo Compiling XrdCommandLine.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdCommandLine.o XrdCommandLine.cc

$(OBJDIR)/XrdClientEnv.o: XrdClientEnv.cc ../XrdClient/XrdClientConst.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh
	@echo Compiling XrdClientEnv.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientEnv.o XrdClientEnv.cc

$(OBJDIR)/XrdCpWorkLst.o: XrdCpWorkLst.cc ../XrdClient/XrdCpWorkLst.hh \
  ../XrdClient/XrdClientAdmin.hh ../XrdClient/XrdClientAbs.hh \
  ../XrdClient/XrdClientUnsolMsg.hh ../XrdClient/XrdClientMessage.hh \
  ../XrdClient/XrdClientProtocol.hh ../XProtocol/XProtocol.hh \
  ../XProtocol/XPtypes.hh ../XrdClient/XrdClientSock.hh \
  ../XrdClient/XrdClientUrlInfo.hh  \
  ../XrdClient/XrdClientConn.hh ../XrdClient/XrdClientConst.hh \
  ../XrdClient/XrdClientReadCache.hh ../XrdClient/XrdClientInputBuffer.hh \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh \
  ../XrdOuc/XrdOucSemWait.hh ../XrdOuc/XrdOucHash.hh \
  ../XrdOuc/XrdOucHash.icc ../XrdClient/XrdClientVector.hh \
  ../XrdClient/XrdClient.hh
	@echo Compiling XrdCpWorkLst.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdCpWorkLst.o XrdCpWorkLst.cc

$(OBJDIR)/XrdClientSid.o: XrdClientSid.cc ../XrdClient/XrdClientSid.hh \
  ../XrdOuc/XrdOucRash.hh ../XrdOuc/XrdOucRash.icc \
  ../XrdSys/XrdSysPlatform.hh ../XProtocol/XProtocol.hh \
  ../XProtocol/XPtypes.hh ../XrdClient/XrdClientVector.hh \
  ../XrdOuc/XrdOucPthread.hh ../XrdOuc/XrdOucError.hh
	@echo Compiling XrdClientSid.cc
	$(ECHO)$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientSid.o XrdClientSid.cc

$(OBJDIR)/XrdClientAdmin.o: XrdClientAdmin.cc ../XrdClient/XrdClientAdmin.hh \
  ../XrdClient/XrdClientAbs.hh ../XrdClient/XrdClientUnsolMsg.hh \
  ../XrdClient/XrdClientMessage.hh ../XrdClient/XrdClientProtocol.hh \
  ../XProtocol/XProtocol.hh ../XProtocol/XPtypes.hh \
  ../XrdClient/XrdClientSock.hh ../XrdClient/XrdClientUrlInfo.hh \
  ../XrdClient/XrdClientConn.hh \
  ../XrdClient/XrdClientConst.hh ../XrdClient/XrdClientReadCache.hh \
  ../XrdClient/XrdClientInputBuffer.hh ../XrdOuc/XrdOucPthread.hh \
  ../XrdOuc/XrdOucError.hh ../XrdOuc/XrdOucSemWait.hh \
  ../XrdOuc/XrdOucHash.hh ../XrdOuc/XrdOucHash.icc \
  ../XrdClient/XrdClientVector.hh ../XrdClient/XrdClientDebug.hh \
  ../XrdClient/XrdClientEnv.hh ../XrdOuc/XrdOucEnv.hh \
  ../XrdOuc/XrdOucLogger.hh ../XrdClient/XrdClientUrlSet.hh \
  ../XrdClient/XrdClientConnMgr.hh ../XrdClient/XrdClientLogConnection.hh \
  ../XrdClient/XrdClientPhyConnection.hh ../XrdClient/XrdClientThread.hh
