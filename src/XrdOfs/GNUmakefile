#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
ARCH    = `/bin/uname`

CC      = g++

DBGSFX  =

INCLUDE = -I. -I..

LIBDIR  = ../../lib/$(ARCH)$(DBGSFX)

OBJDIR  = ../../obj/$(ARCH)$(DBGSFX)

LIBS    = -L$(LIBDIR) -lXrdAcc -lXrdOss -lXrdOdc -lXrdOuc

LIBDEP  = $(LIBDIR)/libXrdAcc.a $(LIBDIR)/libXrdOss.a \
          $(LIBDIR)/libXrdOdc.a $(LIBDIR)/libXrdOuc.a

TYPE    = `/bin/uname`

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdOfs.cc  \
        XrdOfsConfig.cc \
        XrdOfsHandle.cc
  
OBJECTS = \
        $(OBJDIR)/XrdOfs.o \
        $(OBJDIR)/XrdOfsConfig.o  \
        $(OBJDIR)/XrdOfsHandle.o  \

LIBRARY = $(LIBDIR)/libXrdOfs.so

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdOdc% ../XrdOdc
vpath XrdOfs% ../XrdOfs
vpath XrdOss% ../XrdOss
vpath XrdOuc% ../XrdOuc
vpath XrdAcc% ../XrdAcc
vpath XrdSec% ../XrdSec
vpath XrdSfs% ../XrdSfs

#------------------------------------------------------------------------------#
#        A r c h i t e c t u r e   S p e c i f i c   V a r i a b l e s         #
#------------------------------------------------------------------------------#
  
SUN64      = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SUNMT      = -D_REENTRANT -mt -D_POSIX_PTHREAD_SEMANTICS
SUNLDSO    = -KPIC -G
SUNOPT     = -fast -xarch=v8plus
SUNDBG     = -g
SUNCFLAGS  = -DSUNCC $(SUN64) $(SUNMT) $(SUNCCT) $(SUNLDSO)
SUNLDOP    =

LNX64      = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LNXOPT     = -O3
LNXLDSO    = -fPIC -shared
LNXDBG     = -g
LNXCFLAGS  = -D__linux__ -D_ALL_SOURCE $(LNX64) $(LNXLDSO) -D_REENTRANT \
             -D_GNU_SOURCE -Wno-deprecated
LNXLDOP    = -rdynamic

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
LFLAGS="$(LNXLDOP)" \
XLIB="" \
TYPE=Linux

Linuxdbg:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++   \
CFLAGS="$(LNXCFLAGS) $(LNXDBG)" \
LFLAGS="$(LNXLDOP)" \
XLIB="" \
TYPE=Linux

SunOSall:
	@$(MAKE) anything  \
ARCH=$(ARCH) \
CC=CC \
CFLAGS="$(SUNCFLAGS) $(SUNOPT)" \
LFLAGS="$(SUNLDOP)" \
XLIB="-lCstd" \
TYPE=SunOS

SunOSdbg:
	@$(MAKE) anything \
ARCH=$(ARCH) \
CC=CC \
CFLAGS="$(SUNCFLAGS) $(SUNDBG)" \
LFLAGS="$(SUNLDOP)" \
XLIB="-lCstd" \
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

$(TARGETS): $(OBJECTS) $(LIBDEP)
	@echo Creating shared library $(LIBRARY) ...
	@$(CC) $(CFLAGS) $(OBJECTS) $(LFLAGS) $(LIBPATH) $(LIBS) $(XLIB) -o $(LIBRARY)

$(OBJDIR)/XrdOfs.o:  XrdOfs.cc        XrdOfs.hh \
                     XrdOfsConfig.hh  XrdOfsHandle.hh    \
                     XrdOfsOpaque.hh  XrdOfsSecurity.hh     XrdOfsTrace.hh     \
                     XrdOuca2x.hh     XrdOucEnv.hh          XrdOucError.hh     \
                     XrdOucHash.hh    XrdOucLock.hh         XrdOucLogger.hh    \
                     XrdOucNetwork.hh XrdOucPList.hh        XrdOucPthread.hh   \
                     XrdOucTrace.hh   XrdSecInterface.hh    XrdSfsInterface.hh \
                     XrdSfsNative.hh  XrdOss.hh             XrdOucTList.hh     \
                     XrdOdcFinder.hh  XrdAccAuthorize.hh    ../XrdVersion.hh
	@echo Compiling XrdOfs.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOfs.o XrdOfs.cc

$(OBJDIR)/XrdOfsConfig.o:  XrdOfsConfig.cc   XrdOfsConfig.hh \
                           XrdOfs.hh         XrdOfsTrace.hh  \
                           XrdOuca2x.hh      XrdOucError.hh   XrdOucStream.hh \
                           XrdOucNetwork.hh  XrdOucTrace.hh                   \
                           XrdAccAuthorize.hh \
                           XrdOdcFinder.hh
	@echo Compiling XrdOfsConfig.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOfsConfig.o XrdOfsConfig.cc

$(OBJDIR)/XrdOfsHandle.o:  XrdOfsHandle.cc   XrdOfsHandle.hh \
                           XrdOfs.hh         XrdOfsConfig.hh  XrdOfsTrace.hh  \
                           XrdOuca2x.hh      XrdOucError.hh   XrdOucStream.hh \
                           XrdOucTrace.hh                                     \
                           XrdOdcFinder.hh
	@echo Compiling XrdOfsHandle.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOfsHandle.o XrdOfsHandle.cc
