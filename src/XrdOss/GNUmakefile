#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
ARCH    = `/bin/uname`

CC      = g++

DBGSFX  =

INCLUDE = -I. -I..

LIBDIR  = ../lib/$(ARCH)$(DBGSFX)

OBJDIR  = ../obj/$(ARCH)$(DBGSFX)

TYPE    = `/bin/uname`

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdOssApi.cc \
        XrdOssCache.cc \
        XrdOssConfig.cc \
        XrdOssCreate.cc \
        XrdOssLock.cc   \
        XrdOssMSS.cc    \
        XrdOssRename.cc \
        XrdOssStage.cc  \
        XrdOssUnlink.cc
  
OBJECTS = \
        $(OBJDIR)/XrdOssApi.o \
        $(OBJDIR)/XrdOssCache.o \
        $(OBJDIR)/XrdOssConfig.o \
        $(OBJDIR)/XrdOssCreate.o \
        $(OBJDIR)/XrdOssLock.o   \
        $(OBJDIR)/XrdOssMSS.o    \
        $(OBJDIR)/XrdOssRename.o \
        $(OBJDIR)/XrdOssStage.o  \
        $(OBJDIR)/XrdOssUnlink.o

LIBRARY = $(LIBDIR)/libXrdOss.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdOfs% ../XrdOfs
vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#        A r c h i t e c t u r e   S p e c i f i c   V a r i a b l e s         #
#------------------------------------------------------------------------------#
  
SUN64      = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SUNMT      = -D_REENTRANT -mt -D_POSIX_PTHREAD_SEMANTICS
SUNOPT     = -fast -xarch=v8plus
SUNDBG     = -g
SUNCFLAGS  = -DSUNCC $(SUN64) $(SUNMT) $(SUNCCT)

LNX64      = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LNXOPT     = -O3
LNXDBG     = -g
LNXCFLAGS  = -D__linux__ -D_ALL_SOURCE $(LNX64) -D_REENTRANT -D_GNU_SOURCE \
             -Wno-deprecated

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
all:
	@/bin/mkdir -p $(LIBDIR) $(OBJDIR)
	@$(MAKE) $(TYPE)all --no-print-directory

debug:
	@/bin/mkdir -p $(LIBDIR)/dbg $(OBJDIR)/dbg
	@$(MAKE) $(TYPE)dbg DBGSFX=/dbg

Linuxall:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++    \
CFLAGS="$(LNXCFLAGS) $(LNXOPT)" \
TYPE=Linux

Linuxdbg:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++    \
CFLAGS="$(LNXCFLAGS) $(LNXDBG)" \
TYPE=Linux

SunOSall:
	@$(MAKE) anything  \
ARCH=$(ARCH) \
CC=CC \
CFLAGS="$(SUNCFLAGS) $(SUNOPT)" \
TYPE=SunOS

SunOSdbg:
	@$(MAKE) anything \
ARCH=$(ARCH) \
CC=CC \
CFLAGS="$(SUNCFLAGS) $(SUNDBG)" \
TYPE=SunOS

anything: $(TARGETS)
	@echo Make XrdOfs done.

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

$(TARGETS): $(OBJECTS)
	@echo Creating archive $(LIBRARY) ...
	@rm -f $(LIBRARY)
	@ar -rc $(LIBRARY) $(OBJECTS)
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	 ar -rc $(LIBRARY) $(OBJDIR)/SunWS_cache/*/*.o; \
fi

$(OBJDIR)/XrdOssApi.o: XrdOss.hh XrdOssApi.hh XrdOssCache.hh XrdOssConfig.hh \
                       XrdOssError.hh XrdOssLock.hh XrdOssTrace.hh \
                       XrdOucError.hh XrdOucPList.hh XrdOucStream.hh \
                       XrdOucPlatform.hh XrdOssApi.cc
	@echo Compiling XrdOssApi.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssApi.o XrdOssApi.cc

$(OBJDIR)/XrdOssCache.o: XrdOssApi.hh XrdOssCache.hh XrdOssTrace.hh \
                         XrdOucDLlist.hh XrdOucError.hh \
                         XrdOssCache.cc
	@echo Compiling XrdOssCache.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssCache.o XrdOssCache.cc

$(OBJDIR)/XrdOssConfig.o: XrdOssApi.hh XrdOssConfig.hh XrdOssError.hh \
                          XrdOssLock.hh XrdOssTrace.hh XrdOucError.hh \
                          XrdOssConfig.cc XrdOuca2x.hh
	@echo Compiling XrdOssConfig.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssConfig.o XrdOssConfig.cc

$(OBJDIR)/XrdOssCreate.o: XrdOssApi.hh XrdOssConfig.hh  XrdOssError.hh \
                          XrdOssLock.hh XrdOssTrace.hh  XrdOssOpaque.hh \
                          XrdOucEnv.hh  XrdOucError.hh  XrdOuca2x.hh  \
                          XrdOssCreate.cc
	@echo Compiling XrdOssCreate.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssCreate.o XrdOssCreate.cc

$(OBJDIR)/XrdOssLock.o:   XrdOssApi.hh XrdOssConfig.hh XrdOssError.hh \
                          XrdOssLock.hh XrdOucError.hh XrdOucPlatform.hh \
                          XrdOssLock.cc
	@echo Compiling XrdOssLock.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssLock.o XrdOssLock.cc

$(OBJDIR)/XrdOssMSS.o:    XrdOssApi.hh XrdOssConfig.hh XrdOssError.hh \
                          XrdOssTrace.hh XrdOucError.hh XrdOucSocket.hh \
                          XrdOucStream.hh XrdOucPlatform.hh \
                          XrdOssMSS.cc
	@echo Compiling XrdOssMSS.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssMSS.o XrdOssMSS.cc

$(OBJDIR)/XrdOssRename.o: XrdOssApi.hh XrdOssError.hh XrdOssLock.hh \
                          XrdOssTrace.hh \
                          XrdOssRename.cc
	@echo Compiling XrdOssRename.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssRename.o XrdOssRename.cc

$(OBJDIR)/XrdOssStage.o: XrdOssApi.hh XrdOssCache.hh XrdOssError.hh \
                         XrdOssLock.hh  XrdOssOpaque.hh XrdOucEnv.hh \
                         XrdOuca2x.hh XrdOssStage.cc
	@echo Compiling XrdOssStage.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssStage.o XrdOssStage.cc

$(OBJDIR)/XrdOssUnlink.o: XrdOssApi.hh XrdOssError.hh XrdOssLock.hh \
                          XrdOssTrace.hh \
                          XrdOssUnlink.cc
	@echo Compiling XrdOssUnlink.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssUnlink.o XrdOssUnlink.cc
