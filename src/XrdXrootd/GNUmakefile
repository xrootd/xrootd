#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

BINLIBS = -L$(LIBDIR) -lXrd -lXrdNet -lXrdOuc $(OBJDIR)/XrdSfsNative.o

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdXrootdAio.cc        \
        XrdXrootdConfig.cc     \
        XrdXrootdFile.cc       \
        XrdXrootdFileLock1.cc \
        XrdXrootdLoadLib.cc \
        XrdXrootdMonitor.cc   \
        XrdXrootdPrepare.cc   \
        XrdXrootdProtocol.cc   \
        XrdXrootdResponse.cc   \
        XrdXrootdStats.cc      \
        XrdXrootdXeq.cc        \
        XrdXrootdXeqAio.cc
  
OBJECTS = \
        $(OBJDIR)/XrdXrootdAio.o        \
        $(OBJDIR)/XrdXrootdConfig.o     \
        $(OBJDIR)/XrdXrootdFile.o       \
        $(OBJDIR)/XrdXrootdFileLock1.o \
        $(OBJDIR)/XrdXrootdLoadLib.o \
        $(OBJDIR)/XrdXrootdMonitor.o \
        $(OBJDIR)/XrdXrootdPrepare.o \
        $(OBJDIR)/XrdXrootdProtocol.o   \
        $(OBJDIR)/XrdXrootdResponse.o   \
        $(OBJDIR)/XrdXrootdStats.o      \
        $(OBJDIR)/XrdXrootdXeq.o        \
        $(OBJDIR)/XrdXrootdXeqAio.o

OBJDEPS = \
          $(LIBDIR)/libXrd.a \
          $(LIBDIR)/libXrdNet.a \
          $(LIBDIR)/libXrdOuc.a \
          $(OBJDIR)/XrdSfsNative.o

TARGETS  = $(BINDIR)/xrootd

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                           -
#------------------------------------------------------------------------------#

vpath XrdSfs%    ../XrdSfs
vpath XrdSec%    ../XrdSec
vpath XrdOuc%    ../XrdOuc
vpath XrdNet%    ../XrdNet
vpath XrdXrootd% ../XrdXrdXrootd
vpath Xrd%       ../Xrd
vpath XP%        ../XProtocol

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdXrootd done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS) $(OBJDEPS)
	@echo Creating executable $(TARGETS) 
	$(ECHO)$(LD) $(LDOP) $(OBJECTS) $(LIBS) $(BINLIBS) -o $(TARGETS)

$(OBJDIR)/XrdXrootdAio.o: XrdXrootdAio.hh      XPtypes.hh           \
                          XrdBuffer.hh         XrdLink.hh           \
                          XrdScheduler.hh \
                          XrdOucError.hh       XrdOucPthread.hh     \
                          XrdSfsAio.hh         XrdSfsInterface.hh   \
                          XrdXrootdFile.hh     XrdXrootdProtocol.hh \
                          XrdXrootdResponse.hh XrdXrootdStats.hh    \
                          XrdXrootdTrace.hh    XrdXrootdAio.cc
	@echo Compiling XrdXrootdAio.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdAio.o XrdXrootdAio.cc

$(OBJDIR)/XrdXrootdConfig.o: XrdXrootdProtocol.hh XrdXrootdFile.hh \
                             XrdXrootdFileLock.hh XrdXrootdFileLock1.hh \
                             XrdXrootdPrepare.hh  XrdXrootdXPath.hh \
                             XrdXrootdStats.hh    XrdXrootdTrace.hh \
                             XrdXrootdMonitor.hh  XrdXrootdAio.hh \
                             XrdNetDNS.hh \
                             XrdBuffer.hh         XProtocol.hh \
                             XrdOuca2x.hh         XrdOucError.hh  \
                             XrdOucLogger.hh      XrdOucReqID.hh \
                             XrdOucTokenizer.hh   XrdOucTrace.hh \
                             XrdOucProg.hh        XrdXrootdConfig.cc \
                             ../XrdVersion.hh
	@echo Compiling XrdXrootdConfig.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdConfig.o XrdXrootdConfig.cc

$(OBJDIR)/XrdXrootdFile.o:   XrdSfsInterface.hh   XrdXrootdFile.hh \
                             XrdXrootdFileLock.hh \
                             XrdXrootdFile.cc     XrdXrootdTrace.hh \
                             XrdOucError.hh       XrdOucTrace.hh \
                             XrdOucPthread.hh     XPtypes.hh
	@echo Compiling XrdXrootdFile.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdFile.o XrdXrootdFile.cc

$(OBJDIR)/XrdXrootdFileLock1.o: XrdXrootdFile.hh XrdXrootdFileLock1.hh \
                                 XrdOucHash.hh   XrdOucHash.icc XrdOucPthread.hh \
                                 XrdXrootdFileLock1.cc
	@echo Compiling XrdXrootdFileLock1.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdFileLock1.o XrdXrootdFileLock1.cc

$(OBJDIR)/XrdXrootdLoadLib.o: XrdSfsInterface.hh XrdSecInterface.hh XrdOucError.hh \
                              XrdXrootdLoadLib.cc
	@echo Compiling XrdXrootdLoadLib.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdLoadLib.o XrdXrootdLoadLib.cc

$(OBJDIR)/XrdXrootdMonitor.o: XrdOucError.hh      \
                              XrdOucPthread.hh    XrdOucPlatform.hh \
                              XrdNet.hh           XrdNetPeer.hh     \
                              XrdScheduler.hh     XrdJob.hh        \
                              XPtypes.hh          XrdXrootdMonitor.hh \
                              XrdXrootdMonData.hh XrdXrootdMonitor.cc  \
                              XrdXrootdTrace.hh
	@echo Compiling XrdXrootdMonitor.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdMonitor.o XrdXrootdMonitor.cc

$(OBJDIR)/XrdXrootdPrepare.o: XrdOucError.hh      XrdOucTList.hh   \
                              XrdJob.hh           XrdScheduler.hh  \
                              XrdXrootdTrace.hh   XrdOucPlatform.hh \
                              XrdXrootdPrepare.hh XrdXrootdPrepare.cc
	@echo Compiling XrdXrootdPrepare.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdPrepare.o XrdXrootdPrepare.cc

$(OBJDIR)/XrdXrootdProtocol.o: XrdXrootdFile.hh       XrdXrootdFileLock.hh \
                               XrdXrootdFileLock1.hh  XrdXrootdXPath.hh \
                               XrdXrootdProtocol.hh   XrdXrootdResponse.hh \
                               XrdXrootdStats.hh      XrdXrootdTrace.hh    \
                               XrdXrootdAio.hh        XProtocol.hh         \
                               XrdBuffer.hh           XrdLink.hh           \
                               XrdObject.hh           XProtocol.hh \
                               XrdSfsInterface.hh     XrdSecInterface.hh \
                               XrdOucError.hh         XrdOucPthread.hh   \
                               XrdOucTrace.hh         XrdXrootdProtocol.cc
	@echo Compiling XrdXrootdProtocol.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdProtocol.o XrdXrootdProtocol.cc

$(OBJDIR)/XrdXrootdResponse.o: XrdLink.hh           XrdXrootdResponse.hh  \
                               XrdXrootdTrace.hh \
                               XProtocol.hh      XrdOucError.hh \
                               XrdOucTrace.hh \
                               XrdXrootdResponse.cc
	@echo Compiling XrdXrootdResponse.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdResponse.o XrdXrootdResponse.cc

$(OBJDIR)/XrdXrootdStats.o: XrdXrootdResponse.hh XrdStats.hh XrdOucPthread.hh \
                            XrdXrootdStats.cc
	@echo Compiling XrdXrootdStats.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdStats.o XrdXrootdStats.cc

$(OBJDIR)/XrdXrootdXeq.o: XrdXrootdFile.hh      XrdXrootdFileLock.hh  \
                          XrdXrootdProtocol.hh  XrdXrootdStats.hh     \
                          XrdXrootdTrace.hh     XProtocol.hh       \
                          XrdXrootdPrepare.hh   XrdXrootdXPath.hh  \
                          XrdXrootdMonitor.hh   XrdXrootdMonData.hh \
                          XrdXrootdAio.hh \
                          XrdBuffer.hh          XrdLink.hh            \
                          XrdSfsInterface.hh    XrdSecInterface.hh \
                          XrdOucError.hh        XrdOucTrace.hh  \
                          XrdOucPlatform.hh     XrdOucTList.hh \
                          XrdOucReqID.hh        XrdOucStream.hh \
                          XrdOucProg.hh         XrdXrootdXeq.cc
	@echo Compiling XrdXrootdXeq.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdXeq.o XrdXrootdXeq.cc

$(OBJDIR)/XrdXrootdXeqAio.o: XrdXrootdXeqAio.cc \
                             XrdBuffer.hh         XrdLink.hh       \
                             XrdOucError.hh       XrdOucErrInfo.hh \
                             XrdSfsInterface.hh                    \
                             XrdXrootdAio.hh      XrdXrootdFile.hh \
                             XrdXrootdProtocol.hh XrdXrootdTrace.hh
	@echo Compiling XrdXrootdXeqAio.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdXeqAio.o XrdXrootdXeqAio.cc
