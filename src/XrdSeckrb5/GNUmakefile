#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
ARCH    = `/bin/uname`

CC      = g++

DBGSFX  =

INCLUDE = -I. -I.. -I/afs/slac.stanford.edu/package/krb5/include

BINDIR  = ../../bin/$(ARCH)$(DBGSFX)

LIBDIR  = ../../lib/$(ARCH)$(DBGSFX)

OBJDIR  = ../../obj/$(ARCH)$(DBGSFX)

TYPE    = `/bin/uname`

LIBPATH = -L$(LIBDIR) -lXrdOuc

LIBDEPS = $(LIBDIR)/libXrdOuc.a

LIBKRB5 = -L/afs/slac.stanford.edu/package/krb5/lib \
          -lkrb5 -lk5crypto -lcom_err -lresolv


#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdSecProtocolkrb5.cc

OBJECTS = \
        $(OBJDIR)/XrdSecProtocolkrb5.o

TARGETS = $(LIBDIR)/libXrdSeckrb5.so

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc
vpath XrdSec% ../XrdSec

#------------------------------------------------------------------------------#
#        A r c h i t e c t u r e   S p e c i f i c   V a r i a b l e s         #
#------------------------------------------------------------------------------#
  
SUN32      = -DSUN $(SUNMT) $(SUNCCT)
SUN64      = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SUNMT      = -D_REENTRANT -mt -D_POSIX_PTHREAD_SEMANTICS
SUNOPT     = -fast -xarch=v8plus
SUNLDSO    = -KPIC -G
SUNDBG     = -g
SUNCFLAGS  = -DSUN $(SUN64) $(SUNMT) $(SUNCCT)

SUNLIBS    =  -lposix4 -lsocket -lnsl -lpthread -ldl

LNX32      = -D__linux__ -D_ALL_SOURCE -D_REENTRANT -D_GNU_SOURCE \
             -Wno-deprecated
LNX64      = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LNXOPT     = -O3
LNXLDSO    = -fPIC -shared
LNXDBG     = -g
LNXCFLAGS  = -D__linux__ -D_ALL_SOURCE $(LNX64) -D_REENTRANT -D_GNU_SOURCE

LNXLIBS    =  -lnsl -lpthread -lrt -ldl

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
all:
	@/bin/mkdir -p $(LIBDIR) $(OBJDIR) $(BINDIR)
	@$(MAKE) $(TYPE)all --no-print-directory

debug:
	@/bin/mkdir -p $(LIBDIR)_dbg $(OBJDIR)_dbg $(BINDIR)_dbg
	@$(MAKE) $(TYPE)dbg DBGSFX=_dbg

Linuxall:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++    \
CF32="$(LNX32)" \
CFLAGS="$(LNXCFLAGS) $(LNXOPT)" \
SFLAGS="$(LNXLDSO)" \
LIBS="$(LNXLIBS) $(LIBKRB5)" \
TYPE=Linux

Linuxdbg:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++    \
CF32="$(LNX32)" \
CFLAGS="$(LNXCFLAGS) $(LNXDBG)" \
SFLAGS="$(LNXLDSO)" \
LIBS="$(LNXLIBS) $(LIBKRB5)" \
TYPE=Linux

SunOSall:
	@$(MAKE) anything \
ARCH=$(ARCH) \
CC=CC \
CF32="$(SUN32)" \
CFLAGS="$(SUNCFLAGS) $(SUNOPT)" \
SFLAGS="$(SUNLDSO)" \
LIBS="$(SUNLIBS) $(LIBKRB5)" \
TYPE=SunOS

SunOSdbg:
	@$(MAKE) anything \
ARCH=$(ARCH) \
CC=CC \
CF32="$(SUN32)" \
CFLAGS="$(SUNCFLAGS)$(SUNDBG)" \
SFLAGS="$(SUNLDSO)" \
LIBS="$(SUNLIBS) $(LIBKRB5)" \
TYPE=SunOS

anything: $(TARGETS)
	@echo Make XrdSec done.

clean:
	@$(MAKE) remove             --no-print-directory
	@$(MAKE) remove DBGSFX=_dbg --no-print-directory
	@echo Make clean done.

remove: FORCE
	@rm -f $(OBJECTS) $(TARGETS)

FORCE: ;

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS) $(LIBDEPS)
	@echo Creating shared library $(TARGETS) ...
	@$(CC) $(CFLAGS) $(OBJECTS) $(LFLAGS) $(SFLAGS) $(LIBPATH) $(LIBS) -o $(TARGETS)

$(OBJDIR)/XrdSecProtocolkrb5.o: XrdSecProtocolkrb5.cc XrdSecInterface.hh \
                                XrdOucPthread.hh      XrdOucTokenizer.hh \
                                XrdOucErrInfo.hh
	@echo Compiling XrdSecProtocolkrb5.cc ...
	@$(CC) -c $(CFLAGS) $(SFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSecProtocolkrb5.o XrdSecProtocolkrb5.cc
