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

LIBPATH = -L$(LIBDIR) -lXrdOuc

LIBDEPS = $(LIBDIR)/libXrdOuc.a

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdSecClient.cc        \
        XrdSecPManager.cc      \
        XrdSecProtocolsrvr.cc  \
        XrdSectestClient.cc    \
        XrdSectestServer.cc    \
  
OBJSLIB = \
        $(OBJDIR)/XrdSecClient.o        \
        $(OBJDIR)/XrdSecPManager.o      \
        $(OBJDIR)/XrdSecProtocolsrvr.o

OBJECTA = \
        $(OBJDIR)/XrdSectestClient.o

OBJECTB = \
        $(OBJDIR)/XrdSectestServer.o

OBJECTS = $(OBJSLIB) $(OBJECTA) $(OBJECTB)

LIBRARY = $(LIBDIR)/libXrdSec.so

TSTBINA = $(BINDIR)/testclient
TSTBINB = $(BINDIR)/testserver

TARGETS = $(LIBRARY) $(TSTBINA) $(TSTBINB)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#        A r c h i t e c t u r e   S p e c i f i c   V a r i a b l e s         #
#------------------------------------------------------------------------------#
  
SUN32      = -DSUN $(SUNMT) $(SUNCCT)
SUN64      = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SUNMT      = -D_REENTRANT -mt -D_POSIX_PTHREAD_SEMANTICS
SUNOPT     = -fast -xarch=v8plus
SUNLDSO    = -KPIC -G
SUNDBG     = -g -xs
SUNCFLAGS  = -DSUN $(SUN64) $(SUNMT) $(SUNCCT)

SUNLFLAGS  =

SUNLIBS    =  -lposix4 -lsocket -lnsl -lpthread -ldl

LNX32      = -D__linux__ -D_ALL_SOURCE -D_REENTRANT -D_GNU_SOURCE
LNX64      = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LNXOPT     = -O3
LNXLDSO    = -fPIC -shared
LNXDBG     = -g -o3
LNXCFLAGS  = -D__linux__ -D_ALL_SOURCE $(LNX64) -D_REENTRANT -D_GNU_SOURCE \
             -Wno-deprecated
LNXLFLAGS  = -rdynamic
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
LFLAGS="$(LNXLFLAGS)" \
SFLAGS="$(LNXLDSO)" \
LIBS="$(LNXLIBS)" \
TYPE=Linux

Linuxdbg:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++    \
CF32="$(LNX32) $(LNXDBG)" \
CFLAGS="$(LNXCFLAGS) $(LNXDBG)" \
LFLAGS="$(LNXLFLAGS)" \
SFLAGS="$(LNXLDSO)" \
LIBS="$(LNXLIBS)" \
TYPE=Linux

SunOSall:
	@$(MAKE) anything \
ARCH=$(ARCH) \
CC=CC \
CF32="$(SUN32)" \
CFLAGS="$(SUNCFLAGS) $(SUNOPT)" \
LFLAGS="$(SUNLFLAGS)" \
SFLAGS="$(SUNLDSO)" \
LIBS="$(SUNLIBS)" \
TYPE=SunOS

SunOSdbg:
	@$(MAKE) anything \
ARCH=$(ARCH) \
CC=CC \
CF32="$(SUN32) $(SUNDBG)" \
CFLAGS="$(SUNCFLAGS)$(SUNDBG)" \
LFLAGS="$(SUNLFLAGS)" \
SFLAGS="$(SUNLDSO)" \
LIBS="$(SUNLIBS)" \
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

$(LIBRARY): $(OBJSLIB) $(LIBDEPS)
	@echo Creating shared library $(LIBRARY) ...
	@$(CC) $(CFLAGS) $(LFLAGS) $(OBJSLIB) $(LFLAGS) $(SFLAGS) $(LIBPATH) $(LIBS) -o $(LIBRARY)

$(TSTBINA): $(OBJECTA) $(LIBDEPS)
	@echo Creating executable $(TSTBINA) ...
	@$(CC) $(OBJECTA) $(BINLIBS) $(LIBPATH) -lXrdSec $(LIBS) -o $(TSTBINA)

$(TSTBINB): $(OBJECTB) $(LIBDEPS)
	@echo Creating executable $(TSTBINB) ...
	@$(CC) $(OBJECTB) $(BINLIBS) $(LIBPATH) -lXrdSec $(LIBS) -o $(TSTBINB)

$(OBJDIR)/XrdSecClient.o: XrdOucPthread.hh XrdSecPManager.hh XrdSecInterface.hh \
                          XrdOucErrInfo.hh XrdSecClient.cc
	@echo Compiling XrdSecClient.cc ...
	@$(CC) -c $(CFLAGS) $(SFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSecClient.o XrdSecClient.cc

$(OBJDIR)/XrdSecPManager.o: XrdSecInterface.hh XrdSecPManager.hh \
                            XrdSecProtocolhost.hh \
                            XrdOucErrInfo.hh   XrdOucPthread.hh \
                            XrdSecPManager.cc
	@echo Compiling XrdSecPManager.cc ...
	@$(CC) -c $(CFLAGS) $(SFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSecPManager.o XrdSecPManager.cc

$(OBJDIR)/XrdSecProtocolsrvr.o: XrdSecProtocolsrvr.cc XrdSecProtocolsrvr.hh \
                                XrdOucErrInfo.hh      XrdOucError.hh \
                                XrdOucLogger.hh       XrdOucStream.hh \
                                XrdSecInterface.hh    XrdSecPManager.hh \
                                XrdSecTrace.hh
	@echo Compiling XrdSecProtocolsrvr.cc ...
	@$(CC) -c $(CFLAGS) $(SFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSecProtocolsrvr.o XrdSecProtocolsrvr.cc

$(OBJDIR)/XrdSectestClient.o:   XrdSectestClient.cc XrdSecInterface.hh
	@echo Compiling XrdSectestClient.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSectestClient.o XrdSectestClient.cc

$(OBJDIR)/XrdSectestServer.o: XrdSecProtocolsrvr.hh XrdSecInterface.hh XrdOucLogger.hh \
                              XrdSectestServer.cc
	@echo Compiling XrdSectestServer.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSectestServer.o XrdSectestServer.cc
