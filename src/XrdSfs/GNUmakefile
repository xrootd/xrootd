#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
ARCH    = `/bin/uname`

CC      = g++

DBGSFX  =

INCLUDE = -I. -I..

LIBDIR  = ../../lib/$(ARCH)$(DBGSFX)

OBJDIR  = ../../obj/$(ARCH)$(DBGSFX)

TYPE    = `/bin/uname`

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdSfsNative.cc

  
OBJECTS = \
        $(OBJDIR)/XrdSfsNative.o

LIBRARY =

TARGETS = OBJECTFILE

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc
vpath XrdSec% ../XrdSec

#------------------------------------------------------------------------------#
#        A r c h i t e c t u r e   S p e c i f i c   V a r i a b l e s         #
#------------------------------------------------------------------------------#
  
SUN64      = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SUNMT      = -D_REENTRANT -mt -D_POSIX_PTHREAD_SEMANTICS
SUNOPT     = -fast -xarch=v8plus
SUNDBG     = -g
SUNCFLAGS  = -DSUN $(SUN64) $(SUNMT) $(SUNCCT)

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
	@echo Make XrdSfs done.

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
#	@echo Creating archive $(LIBRARY) ...
#	@rm -f $(LIBRARY)
#	@ar -rc $(LIBRARY) $(OBJECTS)
#	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
#	@ar -rc $(LIBRARY) $(OBJDIR)/SunWS_cache/*/*.o; \
#fi

$(OBJDIR)/XrdSfsNative.o:  XrdSfsInterface.hh XrdSfsNative.hh  XrdSecInterface.hh \
                           XrdOucError.hh     XrdOucErrInfo.hh XrdOucLogger.hh \
                           XrdSfsNative.cc    ../XrdVersion.hh
	@echo Compiling XrdSfsNative.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSfsNative.o XrdSfsNative.cc
