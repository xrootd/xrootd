#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
ARCH    = `/bin/uname`

CC      = g++

BINSFX  =

DBGSFX  =

INCLUDE = -I. -I..

BINDIR  = ../bin/$(ARCH)/

BINLIBS = -L../lib/$(ARCH)$(DBGSFX) -lXrd -lXrdOuc \
          ../obj/$(ARCH)$(DBGSFX)/XrdSfsNative.o

LIBDIR  = ../lib/$(ARCH)$(DBGSFX)

OBJDIR  = ../obj/$(ARCH)$(DBGSFX)

TYPE    = `/bin/uname`

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdXrootdConfig.cc     \
        XrdXrootdFile.cc       \
        XrdXrootdFileLock1.cc \
        XrdXrootdLoadLib.cc \
        XrdXrootdPrepare.cc   \
        XrdXrootdProtocol.cc   \
        XrdXrootdResponse.cc   \
        XrdXrootdStats.cc      \
        XrdXrootdXeq.cc
  
OBJECTS = \
        $(OBJDIR)/XrdXrootdConfig.o     \
        $(OBJDIR)/XrdXrootdFile.o       \
        $(OBJDIR)/XrdXrootdFileLock1.o \
        $(OBJDIR)/XrdXrootdLoadLib.o \
        $(OBJDIR)/XrdXrootdPrepare.o \
        $(OBJDIR)/XrdXrootdProtocol.o   \
        $(OBJDIR)/XrdXrootdResponse.o   \
        $(OBJDIR)/XrdXrootdStats.o      \
        $(OBJDIR)/XrdXrootdXeq.o

OBJDEPS = \
          ../lib/$(ARCH)$(DBGSFX)/libXrd.a \
          ../lib/$(ARCH)$(DBGSFX)/libXrdOuc.a \
          ../obj/$(ARCH)$(DBGSFX)/XrdSfsNative.o

TARGET  = $(BINDIR)/xrootd$(BINSFX)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                           -
#------------------------------------------------------------------------------#

vpath XrdSfs% ../XrdSfs
vpath XrdOuc% ../XrdOuc
vpath XrdSec% ../XrdSec
vpath Xrd%    ../Xrd
vpath XP%     ../XProtocol

#------------------------------------------------------------------------------#
#        A r c h i t e c t u r e   S p e c i f i c   V a r i a b l e s         #
#------------------------------------------------------------------------------#

CFLAGS     =  $(CF64) $(CF32) $(CFOP)
  
SUN64      = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SUNMT      = -D_REENTRANT -mt -D_POSIX_PTHREAD_SEMANTICS
SUNOPT     = -fast -xarch=v8plus
SUNDBG     = -g
SUNCFLAGS  = -DSUN $(SUN64) $(SUNMT) $(SUNCCT)
SUN32      = -DSUN $(SUNMT) $(SUNCCT)

SUNLIBS    = -lposix4 -lsocket -lnsl -lpthread -ldl

LNX64      = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LNXOPT     = -O3
LNXDBG     = -g
LNX32      = -D__linux__ -D_ALL_SOURCE -D_REENTRANT -D_GNU_SOURCE \
             -Wno-deprecated
LNXLIBS    =  -lnsl -lpthread -lrt -ldl

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
all:
	@/bin/mkdir -p $(BINDIR) $(LIBDIR) $(OBJDIR)
	@$(MAKE) $(TYPE)all --no-print-directory

debug:
	@/bin/mkdir -p $(BINDIR)/dbg $(LIBDIR)/dbg $(OBJDIR)/dbg
	@$(MAKE) $(TYPE)dbg DBGSFX=/dbg --no-print-directory

Linuxall:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++    \
CF64="$(LNX64)" \
ARCH=$(ARCH) \
CF32="$(LNX32)" \
CFOP="$(LNXOPT)" \
LIBS="$(LNXLIBS)" \
TYPE=Linux

Linuxdbg:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
BINSFX='.dbg' \
CC=g++    \
CF64="$(LNX64)" \
CF32="$(LNX32)" \
CFOP="$(LNXDBG)" \
LIBS="$(LNXLIBS)" \
TYPE=Linux

SunOSall:
	@$(MAKE) anything \
ARCH=$(ARCH) \
CC=CC \
CF64="$(SUN64)" \
CF32="$(SUN32)" \
CFOP="$(SUNOPT) -DSUNCC" \
LIBS="$(SUNLIBS)" \
TYPE=SunOS

SunOSdbg:
	@$(MAKE) anything \
ARCH=$(ARCH) \
BINSFX='.dbg' \
CC=CC \
CF64="$(SUN64)" \
CF32="$(SUN32)" \
CFOP="$(SUNDBG) -DSUNCC" \
LIBS="$(SUNLIBS)" \
TYPE=SunOS

anything: $(TARGET)
	@echo Make xrootd done.

clean:
	@$(MAKE) remove             --no-print-directory
	@$(MAKE) remove DBGSFX=/dbg --no-print-directory
	@echo Make clean done.

remove: FORCE
	@rm -f $(OBJECTS) $(TARGET)

FORCE: ;

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGET): $(OBJECTS) $(OBJDEPS)
	@echo Creating executable $(TARGET) ...
	@$(CC) $(OBJECTS) $(LIBS) $(BINLIBS) -o $(TARGET)

$(OBJDIR)/XrdXrootdConfig.o: XrdXrootdProtocol.hh XrdXrootdFile.hh \
                             XrdXrootdFileLock.hh XrdXrootdFileLock1.hh \
                             XrdXrootdPrepare.hh  XrdXrootdXPath.hh \
                             XrdXrootdStats.hh    XrdXrootdTrace.hh \
                             XrdBuffer.hh         XProtocol.hh \
                             XrdOuca2x.hh         XrdOucError.hh  \
                             XrdOucLogger.hh      XrdOucReqID.hh \
                             XrdOucTokenizer.hh   XrdOucTrace.hh \
                             XrdOucProg.hh        XrdXrootdConfig.cc
	@echo Compiling XrdXrootdConfig.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdConfig.o XrdXrootdConfig.cc

$(OBJDIR)/XrdXrootdFile.o:   XrdSfsInterface.hh   XrdXrootdFile.hh \
                             XrdXrootdFileLock.hh \
                             XrdXrootdFile.cc     XrdXrootdTrace.hh \
                             XrdOucError.hh       XrdOucTrace.hh
	@echo Compiling XrdXrootdFile.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdFile.o XrdXrootdFile.cc

$(OBJDIR)/XrdXrootdFileLock1.o: XrdXrootdFile.hh XrdXrootdFileLock1.hh \
                                 XrdOucHash.hh   XrdOucHash.icc XrdOucPthread.hh \
                                 XrdXrootdFileLock1.cc
	@echo Compiling XrdXrootdFileLock1.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdFileLock1.o XrdXrootdFileLock1.cc

$(OBJDIR)/XrdXrootdLoadLib.o: XrdSfsInterface.hh XrdSecInterface.hh XrdOucError.hh \
                              XrdXrootdLoadLib.cc
	@echo Compiling XrdXrootdLoadLib.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdLoadLib.o XrdXrootdLoadLib.cc

$(OBJDIR)/XrdXrootdPrepare.o: XrdOucError.hh      XrdOucTList.hh   \
                              XrdJob.hh           XrdScheduler.hh  \
                              XrdXrootdTrace.hh   XrdOucPlatform.hh \
                              XrdXrootdPrepare.hh XrdXrootdPrepare.cc
	@echo Compiling XrdXrootdPrepare.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdPrepare.o XrdXrootdPrepare.cc

$(OBJDIR)/XrdXrootdProtocol.o: XrdXrootdFile.hh       XrdXrootdFileLock.hh \
                               XrdXrootdFileLock1.hh  XrdXrootdXPath.hh \
                               XrdXrootdProtocol.hh   XrdXrootdResponse.hh \
                               XrdXrootdStats.hh      XrdXrootdTrace.hh    \
                               XProtocol.hh           \
                               XrdBuffer.hh           XrdLink.hh           \
                               XrdObject.hh           XProtocol.hh \
                               XrdSfsInterface.hh     XrdSecInterface.hh \
                               XrdOucError.hh         XrdOucPthread.hh   \
                               XrdOucTrace.hh         XrdXrootdProtocol.cc
	@echo Compiling XrdXrootdProtocol.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdProtocol.o XrdXrootdProtocol.cc

$(OBJDIR)/XrdXrootdResponse.o: XrdLink.hh           XrdXrootdResponse.hh  \
                               XrdXrootdTrace.hh \
                               XProtocol.hh      XrdOucError.hh \
                               XrdOucTrace.hh \
                               XrdXrootdResponse.cc
	@echo Compiling XrdXrootdResponse.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdResponse.o XrdXrootdResponse.cc

$(OBJDIR)/XrdXrootdStats.o: XrdXrootdResponse.hh XrdStats.hh XrdOucPthread.hh \
                            XrdXrootdStats.cc
	@echo Compiling XrdXrootdStats.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdStats.o XrdXrootdStats.cc

$(OBJDIR)/XrdXrootdXeq.o: XrdXrootdFile.hh      XrdXrootdFileLock.hh  \
                          XrdXrootdProtocol.hh  XrdXrootdStats.hh     \
                          XrdXrootdTrace.hh     XProtocol.hh       \
                          XrdXrootdPrepare.hh   XrdXrootdXPath.hh  \
                          XrdBuffer.hh          XrdLink.hh            \
                          XrdSfsInterface.hh    XrdSecInterface.hh \
                          XrdOucError.hh        XrdOucTrace.hh  \
                          XrdOucPlatform.hh     XrdOucTList.hh \
                          XrdOucReqID.hh        XrdOucStream.hh \
                          XrdOucProg.hh         XrdXrootdXeq.cc
	@echo Compiling XrdXrootdXeq.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrootdXeq.o XrdXrootdXeq.cc
