#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
ARCH    = `/bin/uname`

CC      = g++

DBGSFX  =

INCLUDE = -I. -I..

BINDIR  = ../../bin/$(ARCH)$(DBGSFX)

LIBDIR  = ../../lib/$(ARCH)$(DBGSFX)

OBJDIR  = ../../obj/$(ARCH)$(DBGSFX)

TYPE    = `/bin/uname`

BINLIBS = -L$(LIBDIR) -lXrdOuc

LIBDEPS = $(LIBDIR)/libXrdOuc.a

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

vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#        A r c h i t e c t u r e   S p e c i f i c   V a r i a b l e s         #
#------------------------------------------------------------------------------#
  
SUN64      = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SUNMT      = -D_REENTRANT -mt -D_POSIX_PTHREAD_SEMANTICS
SUNOPT     = -fast -xarch=v8plus
SUNDBG     = -g
SUNCFLAGS  = -DSUN $(SUN64) $(SUNMT) $(SUNCCT)

SUNLIBS    =  -lposix4 -lsocket -lnsl -lpthread

LNX64      = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LNXOPT     = -O3
LNXDBG     = -g
LNXCFLAGS  = -D__linux__ -D_ALL_SOURCE $(LNX64) -D_REENTRANT -D_GNU_SOURCE \
             -Wno-deprecated
LNXLIBS    =  -lnsl -lpthread -lrt

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
all:
	@/bin/mkdir -p $(LIBDIR) $(OBJDIR) $(BINDIR)
	@$(MAKE) $(TYPE)all --no-print-directory

debug:
	@/bin/mkdir -p $(LIBDIR)/dbg $(OBJDIR)/dbg $(BINDIR)/dbg
	@$(MAKE) $(TYPE)dbg DBGSFX=/dbg

Linuxall:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++    \
CFLAGS="$(LNXCFLAGS) $(LNXOPT)" \
LIBS="$(LNXLIBS)" \
TYPE=Linux

Linuxdbg:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++    \
CFLAGS="$(LNXCFLAGS) $(LNXDBG)" \
LIBS="$(LNXLIBS)" \
TYPE=Linux

SunOSall:
	@$(MAKE) anything  \
ARCH=$(ARCH) \
CC=CC \
CFLAGS="$(SUNCFLAGS) $(SUNOPT)" \
LIBS="$(SUNLIBS)" \
TYPE=SunOS

SunOSdbg:
	@$(MAKE) anything \
ARCH=$(ARCH) \
CC=CC \
CFLAGS="$(SUNCFLAGS) $(SUNDBG)" \
LIBS="$(SUNLIBS)" \
TYPE=SunOS

anything: $(TARGETS)
	@echo Make XrdOlb done.

clean:
	@$(MAKE) remove             --no-print-directory
	@$(MAKE) remove DBGSFX=/dbg --no-print-directory
	@echo Make clean done.

remove: FORCE
	@rm -f $(OBJECTS) $(TARGETS)

FORCE: ;

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(BINARY): $(OBJECTS) $(LIBDEPS)
	@echo Creating executable $(BINARY) ...
	@$(CC) $(OBJECTS) $(BINLIBS) $(LIBS) -o $(BINARY)

$(OBJDIR)/XrdOlbAdmin.o: XrdOlbAdmin.hh  XrdOlbAdmin.cc \
                         XrdOlbConfig.hh XrdOlbManager.hh  XrdOlbPrepare.hh \
                         XrdOucError.hh  XrdOucSocket.hh   XrdOucPthread.hh \
                         XrdOuca2x.hh    XrdOucPlatform.hh XrdOucTrace.hh
	@echo Compiling XrdOlbAdmin.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbAdmin.o XrdOlbAdmin.cc

$(OBJDIR)/XrdOlbCache.o: XrdOlbCache.hh XrdOlbPList.hh XrdOlbScheduler.hh \
                         XrdOlbTypes.hh XrdOlbCache.cc \
                         XrdOucLink.hh  XrdOucHash.hh  XrdOucPthread.hh
	@echo Compiling XrdOlbCache.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbCache.o XrdOlbCache.cc

$(OBJDIR)/XrdOlbConfig.o: XrdOlbCache.hh    XrdOlbConfig.hh  XrdOlbManager.hh \
                          XrdOlbMeter.hh    XrdOlbPList.hh   XrdOlbScheduler.hh \
                          XrdOlbTrace.hh    XrdOlbTypes.hh   XrdOlbConfig.cc \
                          XrdOucNetwork.hh  XrdOucPthread.hh XrdOucSecurity.hh \
                          XrdOucStream.hh   XrdOucTList.hh   XrdOlbPrepare.hh \
                          XrdOuca2x.hh      XrdOucLogger.hh  XrdOucError.hh \
                          XrdOucPlatform.hh XrdOucProg.hh    XrdOucSocket.hh \
                          XrdOucTimer.hh    XrdOucTrace.hh
	@echo Compiling XrdOlbConfig.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbConfig.o XrdOlbConfig.cc

$(OBJDIR)/XrdOlbMain.o: XrdOlbCache.hh     XrdOlbConfig.hh   XrdOlbManager.hh \
                        XrdOlbScheduler.hh XrdOlbTrace.hh    XrdOlbMain.cc \
                        XrdOucError.hh     XrdOucLink.hh     XrdOucLogger.hh \
                        XrdOucNetwork.hh   XrdOlbPrepare.hh  XrdOlbAdmin.hh
	@echo Compiling XrdOlbMain.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbMain.o XrdOlbMain.cc

$(OBJDIR)/XrdOlbManager.o: XrdOlbCache.hh     XrdOlbConfig.hh  XrdOlbManager.hh \
                           XrdOlbScheduler.hh XrdOlbServer.hh  XrdOlbTrace.hh \
                           XrdOlbTypes.hh     XrdOlbManager.cc XrdOuca2x.hh \
                           XrdOucHash.hh      XrdOucNetwork.hh XrdOucPthread.hh
	@echo Compiling XrdOlbManager.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbManager.o XrdOlbManager.cc

$(OBJDIR)/XrdOlbMeter.o: XrdOlbMeter.hh XrdOlbMeter.cc    XrdOucPthread.hh \
                         XrdOucError.hh XrdOucPlatform.hh XrdOucTList.hh
	@echo Compiling XrdOlbMeter.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbMeter.o XrdOlbMeter.cc

$(OBJDIR)/XrdOlbPList.o: XrdOlbPList.hh XrdOlbPList.cc XrdOlbTrace.hh \
                         XrdOlbTypes.hh XrdOucPthread.hh
	@echo Compiling XrdOlbPList.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbPList.o XrdOlbPList.cc

$(OBJDIR)/XrdOlbPrepare.o: XrdOlbPrepare.hh XrdOlbPrepare.cc  XrdOucError.hh \
                         XrdOucHash.hh  XrdOucPthread.hh  XrdOlbScheduler.hh \
                         XrdOlbServer.hh XrdOucStream.hh
	@echo Compiling XrdOlbPrepare.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbPrepare.o XrdOlbPrepare.cc

$(OBJDIR)/XrdOlbScheduler.o: XrdOlbScheduler.hh XrdOlbScheduler.cc \
                             XrdOlbTrace.hh \
                             XrdOucChain.hh     XrdOucError.hh   XrdOucLink.hh \
                             XrdOucPthread.hh
	@echo Compiling XrdOlbScheduler.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbScheduler.o XrdOlbScheduler.cc

$(OBJDIR)/XrdOlbServer.o: XrdOlbCache.hh    XrdOlbConfig.hh XrdOlbManager.hh \
                          XrdOlbMeter.hh    XrdOlbServer.hh XrdOlbServer.cc \
                          XrdOlbTrace.hh    XrdOlbTypes.hh  XrdOuca2x.hh \
                          XrdOucHash.hh     XrdOucLink.hh   XrdOucPthread.hh \
                          XrdOucPlatform.hh XrdOucProg.hh   XrdOucStream.hh
	@echo Compiling XrdOlbServer.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOlbServer.o XrdOlbServer.cc
