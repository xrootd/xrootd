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
	XrdAbsClientBase.cc	\
	XrdClient.cc		\
	XrdClientSock.cc	\
	XrdConn.cc		\
	XrdConnMgr.cc		\
	XrdDebug.cc		\
	XrdInputBuffer.cc	\
	XrdLogConnection.cc	\
	XrdMessage.cc		\
	XrdPhyConnection.cc	\
	XrdReadCache.cc		\
	XrdUrlInfo.cc		\
	XrdUrlSet.cc		\
	XrdClientStringMatcher.cc\
	XrdXProtocol.cc

  
OBJECTS = \
	$(OBJDIR)/XrdAbsClientBase.o	\
	$(OBJDIR)/XrdClient.o		\
	$(OBJDIR)/XrdClientSock.o	\
	$(OBJDIR)/XrdConn.o		\
	$(OBJDIR)/XrdConnMgr.o		\
	$(OBJDIR)/XrdDebug.o		\
	$(OBJDIR)/XrdInputBuffer.o	\
	$(OBJDIR)/XrdLogConnection.o	\
	$(OBJDIR)/XrdMessage.o		\
	$(OBJDIR)/XrdPhyConnection.o	\
	$(OBJDIR)/XrdReadCache.o	\
	$(OBJDIR)/XrdUrlInfo.o		\
	$(OBJDIR)/XrdUrlSet.o		\
	$(OBJDIR)/XrdXProtocol.o	\
	$(OBJDIR)/XrdClientStringMatcher.o

OBJECTB = \
        $(OBJDIR)/TestXrdClient.o

LIBRARY = $(LIBDIR)/libXrdClient.a

TESTBIN = $(BINDIR)/TestXrdClient

TARGETS = $(LIBRARY) $(TESTBIN)

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

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS)
	@echo Creating archive $(LIBRARY) 
	@rm -f $(LIBRARY);
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	$(CC) -xar -o $(LIBRARY) $(OBJECTS); \
else \
	ar -rc $(LIBRARY) $(OBJECTS); \
fi

$(TESTBIN): $(OBJECTB) $(LIBDEPS)
	@echo Creating executable $(TESTBIN) 
	$(CC) $(OBJECTB) $(BINLIBS) $(LIBS) -o $(TESTBIN)

$(OBJDIR)/XrdAbsClientBase.o:    XrdAbsClientBase.cc    XrdAbsClientBase.hh\
                                 XrdDebug.cc            XrdDebug.hh
	@echo Compiling XrdBuffer.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdAbsClientBase.o XrdAbsClientBase.cc

$(OBJDIR)/XrdClient.o:           XrdClient.cc           XrdClient.hh\
                                 XrdConn.cc             XrdConn.hh\
                                 XrdConnMgr.cc          XrdConnMgr.hh\
                                 XrdMessage.cc          XrdMessage.hh\
                                 XrdPhyConnection.cc    XrdPhyConnection.hh\
                                 XrdInputBuffer.cc      XrdInputBuffer.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh            XrdReadCache.cc\
                                 XrdReadCache.hh
	@echo Compiling XrdClient.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClient.o XrdClient.cc

$(OBJDIR)/XrdClientSock.o:       XrdClientSock.cc       XrdClientSock.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh
	@echo Compiling XrdClientSock.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientSock.o XrdClientSock.cc

$(OBJDIR)/XrdConn.o:             XrdConn.cc             XrdConn.hh\
                                 XrdConnMgr.cc          XrdConnMgr.hh\
                                 XrdMessage.cc          XrdMessage.hh\
                                 XrdPhyConnection.cc    XrdPhyConnection.hh\
                                 XrdInputBuffer.cc      XrdInputBuffer.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh            XrdReadCache.cc\
                                 XrdReadCache.hh        XrdClientStringMatcher.cc\
                                 XrdClientStringMatcher.hh

	@echo Compiling XrdConn.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdConn.o XrdConn.cc

$(OBJDIR)/XrdConnMgr.o:          XrdConnMgr.cc          XrdConnMgr.hh\
                                 XrdMessage.cc          XrdMessage.hh\
                                 XrdPhyConnection.cc    XrdPhyConnection.hh\
                                 XrdInputBuffer.cc      XrdInputBuffer.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh
	@echo Compiling XrdConnMgr.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdConnMgr.o XrdConnMgr.cc

$(OBJDIR)/XrdDebug.o:            XrdDebug.cc            XrdDebug.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh
	@echo Compiling XrdDebug.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdDebug.o XrdDebug.cc

$(OBJDIR)/XrdInputBuffer.o:      XrdInputBuffer.cc      XrdInputBuffer.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh
	@echo Compiling XrdInputBuffer.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdInputBuffer.o XrdInputBuffer.cc

$(OBJDIR)/XrdLogConnection.o:    XrdLogConnection.cc    XrdLogConnection.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh
	@echo Compiling XrdLogConnection.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdLogConnection.o XrdLogConnection.cc

$(OBJDIR)/XrdMessage.o:          XrdMessage.cc          XrdMessage.hh\
                                 XrdPhyConnection.cc    XrdPhyConnection.hh\
                                 XrdInputBuffer.cc      XrdInputBuffer.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh
	@echo Compiling XrdMessage.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdMessage.o XrdMessage.cc

$(OBJDIR)/XrdPhyConnection.o:    XrdPhyConnection.cc    XrdPhyConnection.hh\
                                 XrdInputBuffer.cc      XrdInputBuffer.hh\
                                 XrdClientSock.cc       XrdClientSock.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh            XrdReadCache.cc\
			 	 XrdReadCache.hh
	@echo Compiling XrdPhyConnection.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPhyConnection.o XrdPhyConnection.cc

$(OBJDIR)/XrdReadCache.o:        XrdReadCache.cc        XrdReadCache.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh
	@echo Compiling XrdReadCache.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdReadCache.o XrdReadCache.cc

$(OBJDIR)/XrdUrlInfo.o:          XrdUrlInfo.cc          XrdUrlInfo.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh
	@echo Compiling XrdUrlInfo.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdUrlInfo.o XrdUrlInfo.cc

$(OBJDIR)/XrdUrlSet.o:           XrdUrlSet.cc           XrdUrlSet.hh\
                                 XrdUrlInfo.cc          XrdUrlInfo.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh
	@echo Compiling XrdUrlSet.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdUrlSet.o XrdUrlSet.cc

$(OBJDIR)/XrdXProtocol.o:        XrdXProtocol.cc        XrdXProtocol.hh\
                                 XrdDebug.cc            XrdDebug.hh\
                                 XrdConst.hh
	@echo Compiling XrdXProtocol.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXProtocol.o XrdXProtocol.cc

$(OBJDIR)/TestXrdClient.o:       TestXrdClient.cc\
                                 XrdConst.hh
	@echo Compiling TestXrdClient.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/TestXrdClient.o TestXrdClient.cc

$(OBJDIR)/XrdClientStringMatcher.o:      XrdClientStringMatcher.cc\
                                         XrdClientStringMatcher.hh
	@echo Compiling XrdClientStringMatcher.cc
	@$(CC) -g -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdClientStringMatcher.o XrdClientStringMatcher.cc
