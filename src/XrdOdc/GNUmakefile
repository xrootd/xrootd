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

BINLIBS = -L$(LIBDIR) -lXrdOdc -lXrdOuc

LIBDEPS = $(LIBDIR)/libXrdOdc.a $(LIBDIR)/libXrdOuc.a

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdOdcConfig.cc \
        XrdOdcFinder.cc \
        XrdOdcManager.cc \
        XrdOdcMsg.cc \
        XrdOdcTest.cc
  
OBJECTA = \
        $(OBJDIR)/XrdOdcConfig.o \
        $(OBJDIR)/XrdOdcFinder.o \
        $(OBJDIR)/XrdOdcManager.o \
        $(OBJDIR)/XrdOdcMsg.o

OBJECTB = \
        $(OBJDIR)/XrdOdcTest.o

OBJECTS = $(OBJECTA) $(OBJECTB)

LIBRARY = $(LIBDIR)/libXrdOdc.a

TESTBIN = $(BINDIR)/test_odc

TARGETS = $(LIBRARY) $(TESTBIN)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdOdc% ../XrdOdc
vpath XrdOuc% ../XrdOuc
vpath XrdSfs% ../XrdSfs

#------------------------------------------------------------------------------#
#        A r c h i t e c t u r e   S p e c i f i c   V a r i a b l e s         #
#------------------------------------------------------------------------------#
  
SUN64      = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SUNMT      = -D_REENTRANT -mt -D_POSIX_PTHREAD_SEMANTICS
SUNOPT     = -fast -xarch=v8plus
SUNDBG     = -g -xs
SUNCFLAGS  = -DSUN $(SUN64) $(SUNMT) $(SUNCCT)

SUNLIBS    =  -lposix4 -lsocket -lnsl -lpthread

LNX64      = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LNXOPT     = -O3
LNXDBG     = -g -O3
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
	@echo Make XrdOdc done.

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

$(LIBRARY): $(OBJECTA)
	@echo Creating archive $(LIBRARY) ...
	@rm -f $(LIBRARY)
	@ar -rc $(LIBRARY) $(OBJECTA)
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	 ar -rc $(LIBRARY) $(OBJDIR)/SunWS_cache/*/*.o; \
fi

$(TESTBIN): $(OBJECTB) $(LIBDEPS)
	@echo Creating executable $(TESTBIN) ...
	@$(CC) $(OBJECTB) $(BINLIBS) $(LIBS) -o $(TESTBIN)

$(OBJDIR)/XrdOdcConfig.o: XrdOdcConfig.hh XrdOdcMsg.hh     XrdOdcTrace.hh \
                          XrdOuca2x.hh    XrdOucNetwork.hh XrdOucStream.hh \
                          XrdOucTList.hh  \
                          XrdOdcConfig.cc XrdOdcConfDefs.hh
	@echo Compiling XrdOdcConfig.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcConfig.o XrdOdcConfig.cc

$(OBJDIR)/XrdOdcFinder.o: XrdOdcFinder.cc   XrdOdcFinder.hh XrdOdcManager.hh \
                          XrdOdcMsg.hh      XrdOdcTrace.hh  XrdOucNetwork.hh \
                          XrdOucPthread.hh  XrdOdcConfDefs.hh XrdOucError.hh \
                          XrdOucErrInfo.hh  XrdOucPlatform.hh XrdOucTimer.hh \
                          XrdOucReqID.hh    XrdOucSocket.hh   XrdOucStream.hh \
                          XrdSfsInterface.hh
	@echo Compiling XrdOdcFinder.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcFinder.o XrdOdcFinder.cc

$(OBJDIR)/XrdOdcManager.o: XrdOdcManager.hh  XrdOdcMsg.hh  XrdOdcTrace.hh \
                           XrdOucError.hh    XrdOucLink.hh XrdOucNetwork.hh \
                           XrdOucPthread.hh \
                           XrdOdcManager.cc
	@echo Compiling XrdOdcManager.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcManager.o XrdOdcManager.cc

$(OBJDIR)/XrdOdcMsg.o: XrdOdcMsg.hh     XrdOdcTrace.hh  XrdOucErrInfo.hh \
                       XrdOucPthread.hh \
                       XrdOdcMsg.cc
	@echo Compiling XrdOdcMsg.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcMsg.o XrdOdcMsg.cc

$(OBJDIR)/XrdOdcTest.o: XrdOucError.hh   XrdOdcFinder.hh  XrdOucErrInfo.hh \
                        XrdOucLogger.hh  XrdOucStream.hh   \
                        XrdOdcTest.cc
	@echo Compiling XrdOdcTest.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcTest.o XrdOdcTest.cc
