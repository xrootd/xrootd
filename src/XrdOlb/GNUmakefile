#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

BINLIBS = -L$(LIBDIR) -lXrdNet -lXrdOuc

LIBDEPS = $(LIBDIR)/libXrdOuc.a $(LIBDIR)/libXrdNet.a

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdOlbAdmin.cc     \
        XrdOlbCache.cc     \
        XrdOlbConfig.cc    \
        XrdOlbMain.cc      \
        XrdOlbManager.cc   \
        XrdOlbMeter.cc     \
        XrdOlbPList.cc     \
        XrdOlbPrepare.cc   \
        XrdOlbScheduler.cc \
        XrdOlbServer.cc
  
OBJECTS = \
        $(OBJDIR)/XrdOlbAdmin.o     \
        $(OBJDIR)/XrdOlbCache.o     \
        $(OBJDIR)/XrdOlbConfig.o    \
        $(OBJDIR)/XrdOlbMain.o      \
        $(OBJDIR)/XrdOlbManager.o   \
        $(OBJDIR)/XrdOlbMeter.o     \
        $(OBJDIR)/XrdOlbPList.o     \
        $(OBJDIR)/XrdOlbPrepare.o   \
        $(OBJDIR)/XrdOlbScheduler.o \
        $(OBJDIR)/XrdOlbServer.o

BINARY  = $(BINDIR)/olbd

TARGETS = $(BINARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdNet% ../XrdNet
vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdOlb done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(BINARY): $(OBJECTS) $(LIBDEPS)
	@echo Creating executable $(BINARY) 
	$(ECHO)$(LD) $(LDOP) $(OBJECTS) $(BINLIBS) $(LIBS) -o $(BINARY)

$(OBJDIR)/XrdOlbAdmin.o: XrdOlbAdmin.hh  XrdOlbAdmin.cc \
                         XrdOlbConfig.hh XrdOlbManager.hh  XrdOlbPrepare.hh \
                         XrdOucError.hh  XrdNetSocket.hh   XrdOucPthread.hh \
                         XrdOuca2x.hh    XrdOucPlatform.hh XrdOucTrace.hh
	@echo Compiling XrdOlbAdmin.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbAdmin.o XrdOlbAdmin.cc

$(OBJDIR)/XrdOlbCache.o: XrdOlbCache.hh XrdOlbPList.hh XrdOlbScheduler.hh \
                         XrdOlbTypes.hh XrdOlbCache.cc \
                         XrdOucHash.hh  XrdOucPthread.hh
	@echo Compiling XrdOlbCache.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbCache.o XrdOlbCache.cc

$(OBJDIR)/XrdOlbConfig.o: XrdOlbCache.hh    XrdOlbConfig.hh  XrdOlbManager.hh \
                          XrdOlbMeter.hh    XrdOlbPList.hh   XrdOlbScheduler.hh \
                          XrdOlbTrace.hh    XrdOlbTypes.hh   XrdOlbConfig.cc \
                          XrdNetDNS.hh      XrdNetLink.hh    XrdNetSecurity.hh \
                          XrdNetSocket.hh   XrdNetWork.hh \
                          XrdOucStream.hh   XrdOucTList.hh   XrdOlbPrepare.hh \
                          XrdOuca2x.hh      XrdOucLogger.hh  XrdOucError.hh \
                          XrdOucPlatform.hh XrdOucProg.hh    XrdOucPthread.hh \
                          XrdOucTimer.hh    XrdOucTrace.hh   \
                          ../XrdVersion.hh
	@echo Compiling XrdOlbConfig.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbConfig.o XrdOlbConfig.cc

$(OBJDIR)/XrdOlbMain.o: XrdOlbCache.hh     XrdOlbConfig.hh   XrdOlbManager.hh \
                        XrdOlbScheduler.hh XrdOlbTrace.hh    XrdOlbMain.cc \
                        XrdOlbPrepare.hh   XrdOlbAdmin.hh \
                        XrdOucError.hh     XrdOucLogger.hh \
                        XrdNetLink.hh      XrdNetWork.hh
	@echo Compiling XrdOlbMain.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbMain.o XrdOlbMain.cc

$(OBJDIR)/XrdOlbManager.o: XrdOlbCache.hh     XrdOlbConfig.hh  XrdOlbManager.hh \
                           XrdOlbScheduler.hh XrdOlbServer.hh  XrdOlbTrace.hh \
                           XrdOlbTypes.hh     XrdOlbManager.cc XrdOuca2x.hh \
                           XrdNetWork.hh    XrdOucPthread.hh
	@echo Compiling XrdOlbManager.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbManager.o XrdOlbManager.cc

$(OBJDIR)/XrdOlbMeter.o: XrdOlbMeter.hh XrdOlbMeter.cc    XrdOucPthread.hh \
                         XrdOucError.hh XrdOucPlatform.hh XrdOucTList.hh
	@echo Compiling XrdOlbMeter.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbMeter.o XrdOlbMeter.cc

$(OBJDIR)/XrdOlbPList.o: XrdOlbPList.hh XrdOlbPList.cc XrdOlbTrace.hh \
                         XrdOlbTypes.hh XrdOucPthread.hh
	@echo Compiling XrdOlbPList.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbPList.o XrdOlbPList.cc

$(OBJDIR)/XrdOlbPrepare.o: XrdOlbPrepare.hh XrdOlbPrepare.cc  XrdOucError.hh \
                         XrdOucHash.hh  XrdOucPthread.hh  XrdOlbScheduler.hh \
                         XrdOlbServer.hh XrdOucStream.hh
	@echo Compiling XrdOlbPrepare.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbPrepare.o XrdOlbPrepare.cc

$(OBJDIR)/XrdOlbScheduler.o: XrdOlbScheduler.hh XrdOlbScheduler.cc \
                             XrdOlbTrace.hh     XrdNetLink.hh \
                             XrdOucChain.hh     XrdOucError.hh   XrdOucPthread.hh
	@echo Compiling XrdOlbScheduler.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbScheduler.o XrdOlbScheduler.cc

$(OBJDIR)/XrdOlbServer.o: XrdOlbServer.hh   XrdOlbServer.cc \
                          XrdOlbCache.hh    XrdOlbConfig.hh \
                          XrdOlbManager.hh  XrdOlbMeter.hh  \
                          XrdOlbTrace.hh    XrdOlbTypes.hh  \
                          XrdNetLink.hh     XrdOuca2x.hh \
                          XrdOucPthread.hh \
                          XrdOucPlatform.hh XrdOucProg.hh   XrdOucStream.hh
	@echo Compiling XrdOlbServer.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbServer.o XrdOlbServer.cc
