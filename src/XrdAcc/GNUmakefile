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

BINLIBS = -L$(LIBDIR) -lXrdAcc -lXrdOuc

LIBDEPS = $(LIBDIR)/libXrdAcc.a $(LIBDIR)/libXrdOuc.a

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdAccAccess.cc      \
        XrdAccAudit.cc       \
        XrdAccAuthFile.cc    \
        XrdAccCapability.cc  \
        XrdAccConfig.cc      \
        XrdAccGroups.cc      \
        XrdAccTest.cc
  
OBJECTA = \
        $(OBJDIR)/XrdAccAccess.o      \
        $(OBJDIR)/XrdAccAudit.o       \
        $(OBJDIR)/XrdAccAuthFile.o    \
        $(OBJDIR)/XrdAccCapability.o  \
        $(OBJDIR)/XrdAccConfig.o      \
        $(OBJDIR)/XrdAccGroups.o

OBJECTB = \
        $(OBJDIR)/XrdAccTest.o

OBJECTS = $(OBJECTA) $(OBJECTB)

LIBRARY = $(LIBDIR)/libXrdAcc.a

TESTBIN = $(BINDIR)/testaccess

TARGETS = $(LIBRARY) $(TESTBIN)

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
	@echo Make XrdAcc done.

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

$(OBJDIR)/XrdAccAccess.o: XrdAccAccess.cc     XrdAccAccess.hh \
                          XrdAccCapability.hh XrdAccConfig.hh    XrdAccGroups.hh \
                          XrdAccAudit.hh     XrdAccAuthorize.hh  XrdAccPrivs.hh \
                          XrdOucError.hh     XrdOucHash.hh       XrdOucLogger.hh \
                          XrdOucXSLock.hh
	@echo Compiling XrdAccAccess.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdAccAccess.o XrdAccAccess.cc

$(OBJDIR)/XrdAccAudit.o: XrdAccAudit.cc XrdAccAudit.hh XrdOucError.hh
	@echo Compiling XrdAccAudit.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdAccAudit.o XrdAccAudit.cc

$(OBJDIR)/XrdAccAuthFile.o: XrdAccAuthFile.hh XrdAccAuthFile.cc \
                            XrdOucError.hh  XrdOucPthread.hh  XrdOucStream.hh \
                            XrdAccAuthDB.hh
	@echo Compiling XrdAccAuthFile.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdAccAuthFile.o XrdAccAuthFile.cc

$(OBJDIR)/XrdAccCapability.o: XrdAccCapability.hh XrdAccCapability.cc \
                              XrdAccPrivs.hh
	@echo Compiling XrdAccCapability.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdAccCapability.o XrdAccCapability.cc

$(OBJDIR)/XrdAccConfig.o: XrdAccConfig.hh     XrdAccConfig.cc \
                          XrdAccAccess.hh     XrdAccAudit.hh  XrdAccAuthDB.hh \
                          XrdAccCapability.hh XrdAccGroups.hh XrdOucLock.hh \
                          XrdOuca2x.hh        XrdOucError.hh  XrdOucHash.hh \
                          XrdOucPthread.hh    XrdOucStream.hh XrdOucStream.hh
	@echo Compiling XrdAccConfig.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdAccConfig.o XrdAccConfig.cc

$(OBJDIR)/XrdAccGroups.o: XrdAccGroups.hh     XrdAccGroups.cc \
                          XrdAccCapability.hh XrdAccPrivs.hh \
                          XrdOucHash.hh       XrdOucPthread.hh
	@echo Compiling XrdAccGroups.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdAccGroups.o XrdAccGroups.cc

$(OBJDIR)/XrdAccTest.o: XrdAccAuthorize.hh XrdAccConfig.hh XrdAccGroups.hh \
                        XrdAccPrivs.hh     XrdOucStream.hh XrdOucError.hh \
                        XrdOucLogger.hh    XrdAccTest.cc
	@echo Compiling XrdAccTest.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdAccTest.o XrdAccTest.cc
