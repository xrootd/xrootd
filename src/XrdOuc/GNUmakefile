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
        XrdOuca2x.cc         \
        XrdOucBuffer.cc      \
        XrdOucEnv.cc         \
        XrdOucError.cc       \
        XrdOucHashVal.cc     \
        XrdOucLink.cc        \
        XrdOucLogger.cc      \
        XrdOucNetwork.cc     \
        XrdOucNList.cc       \
        XrdOucPlatform.cc    \
        XrdOucProg.cc        \
        XrdOucPthread.cc     \
        XrdOucReqID.cc       \
        XrdOucSecurity.cc    \
        XrdOucSocket.cc      \
        XrdOucStream.cc      \
        XrdOucTimer.cc       \
        XrdOucTokenizer.cc   \
        XrdOucTrace.cc       \
        XrdOucXSLock.cc

  
OBJECTS = \
        $(OBJDIR)/XrdOuca2x.o          \
        $(OBJDIR)/XrdOucBuffer.o       \
        $(OBJDIR)/XrdOucError.o        \
        $(OBJDIR)/XrdOucHashVal.o      \
        $(OBJDIR)/XrdOucEnv.o          \
        $(OBJDIR)/XrdOucLogger.o       \
        $(OBJDIR)/XrdOucLink.o         \
        $(OBJDIR)/XrdOucNetwork.o      \
        $(OBJDIR)/XrdOucNList.o        \
        $(OBJDIR)/XrdOucPlatform.o     \
        $(OBJDIR)/XrdOucProg.o         \
        $(OBJDIR)/XrdOucPthread.o      \
        $(OBJDIR)/XrdOucReqID.o        \
        $(OBJDIR)/XrdOucSecurity.o     \
        $(OBJDIR)/XrdOucSocket.o       \
        $(OBJDIR)/XrdOucStream.o       \
        $(OBJDIR)/XrdOucTimer.o        \
        $(OBJDIR)/XrdOucTokenizer.o    \
        $(OBJDIR)/XrdOucTrace.o        \
        $(OBJDIR)/XrdOucXSLock.o

LIBRARY = $(LIBDIR)/libXrdOuc.a

TARGETS = $(LIBRARY)
  
#------------------------------------------------------------------------------#
#        A r c h i t e c t u r e   S p e c i f i c   V a r i a b l e s         #
#------------------------------------------------------------------------------#
  
SUN64      = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SUNMT      = -D_REENTRANT -mt -D_POSIX_PTHREAD_SEMANTICS
SUNOPT     = -fast -xarch=v8plus
SUNDBG     = -g -xs
SUNCFLAGS  = -DSUN $(SUN64) $(SUNMT) $(SUNCCT)

LNX64      = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LNXOPT     = -O3
LNXDBG     = -g -O3
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
	@$(MAKE) anything \
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
	@echo Make oouc done.

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
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	$(CC) -xar -o $(LIBRARY) $(OBJECTS); \
	else \
	ar -rc $(LIBRARY) $(OBJECTS); \
fi

$(OBJDIR)/XrdOuca2x.o: XrdOuca2x.cc XrdOuca2x.hh XrdOucError.hh
	@echo Compiling XrdOuca2x.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOuca2x.cc -o $(OBJDIR)/XrdOuca2x.o

$(OBJDIR)/XrdOucBuffer.o: XrdOucBuffer.cc XrdOucBuffer.hh \
                          XrdOucChain.hh  XrdOucPthread.hh
	@echo Compiling XrdOucBuffer.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucBuffer.cc -o $(OBJDIR)/XrdOucBuffer.o

$(OBJDIR)/XrdOucEnv.o: XrdOucEnv.cc XrdOucEnv.hh XrdOucHash.hh
	@echo Compiling XrdOucEnv.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucEnv.cc -o $(OBJDIR)/XrdOucEnv.o

$(OBJDIR)/XrdOucError.o: XrdOucError.cc XrdOucError.hh XrdOucLogger.hh
	@echo Compiling XrdOucError.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucError.cc -o $(OBJDIR)/XrdOucError.o

$(OBJDIR)/XrdOucHashVal.o: XrdOucHashVal.cc
	@echo Compiling XrdOucHashVal.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucHashVal.cc -o $(OBJDIR)/XrdOucHashVal.o

$(OBJDIR)/XrdOucLink.o: XrdOucLink.cc    XrdOucLink.hh \
                        XrdOucBuffer.hh  XrdOucError.hh  XrdOucLogger.hh \
                        XrdOucNetwork.hh XrdOucStream.hh XrdOucChain.hh \
                        XrdOucPthread.hh
	@echo Compiling XrdOucLink.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucLink.cc -o $(OBJDIR)/XrdOucLink.o

$(OBJDIR)/XrdOucLogger.o: XrdOucLogger.cc XrdOucLogger.hh \
                          XrdOucTimer.hh  XrdOucPthread.hh
	@echo Compiling XrdOucLogger.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucLogger.cc -o $(OBJDIR)/XrdOucLogger.o

$(OBJDIR)/XrdOucNetwork.o: XrdOucNetwork.cc XrdOucNetwork.hh  XrdOucPthread.hh \
                           XrdOucBuffer.hh  XrdOucError.hh    XrdOucLogger.hh \
                           XrdOucLink.hh    XrdOucPlatform.hh XrdOucSecurity.hh \
                           XrdOucTrace.hh
	@echo Compiling XrdOucNetwork.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucNetwork.cc -o $(OBJDIR)/XrdOucNetwork.o

$(OBJDIR)/XrdOucNList.o: XrdOucNList.cc XrdOucNList.hh XrdOucPthread.hh
	@echo Compiling XrdOucNList.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucNList.cc -o $(OBJDIR)/XrdOucNList.o

$(OBJDIR)/XrdOucPlatform.o: XrdOucPlatform.cc XrdOucPlatform.hh
	@echo Compiling XrdOucPlatform.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucPlatform.cc -o $(OBJDIR)/XrdOucPlatform.o

$(OBJDIR)/XrdOucProg.o: XrdOucProg.cc  XrdOucProg.hh \
                        XrdOucError.hh XrdOucStream.hh XrdOucPthread.hh
	@echo Compiling XrdOucProg.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucProg.cc -o $(OBJDIR)/XrdOucProg.o

$(OBJDIR)/XrdOucPthread.o: XrdOucPthread.cc XrdOucPthread.hh 
	@echo Compiling XrdOucPthread.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucPthread.cc -o $(OBJDIR)/XrdOucPthread.o

$(OBJDIR)/XrdOucReqID.o: XrdOucReqID.cc XrdOucReqID.hh XrdOucPthread.hh
	@echo Compiling XrdOucReqID.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucReqID.cc -o $(OBJDIR)/XrdOucReqID.o
                         
$(OBJDIR)/XrdOucSecurity.o: XrdOucSecurity.cc XrdOucSecurity.hh \
                            XrdOucHash.hh XrdOucHash.icc \
                            XrdOucNList.hh XrdOucPthread.hh XrdOucTrace.hh
	@echo Compiling XrdOucSecurity.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucSecurity.cc -o $(OBJDIR)/XrdOucSecurity.o

$(OBJDIR)/XrdOucSocket.o: XrdOucSocket.cc XrdOucSocket.hh XrdOucError.hh \
                          XrdOucPlatform.hh
	@echo Compiling XrdOucSocket.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucSocket.cc -o $(OBJDIR)/XrdOucSocket.o

$(OBJDIR)/XrdOucStream.o: XrdOucStream.cc XrdOucStream.hh XrdOucError.hh
	@echo Compiling XrdOucStream.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucStream.cc -o $(OBJDIR)/XrdOucStream.o

$(OBJDIR)/XrdOucTimer.o: XrdOucTimer.cc XrdOucTimer.hh  
	@echo Compiling XrdOucTimer.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucTimer.cc -o $(OBJDIR)/XrdOucTimer.o

$(OBJDIR)/XrdOucTokenizer.o: XrdOucTokenizer.cc XrdOucTokenizer.hh  
	@echo Compiling XrdOucTokenizer.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucTokenizer.cc -o $(OBJDIR)/XrdOucTokenizer.o

$(OBJDIR)/XrdOucTrace.o: XrdOucError.hh XrdOucTrace.hh XrdOucTrace.cc
	@echo Compiling XrdOucTrace.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucTrace.cc -o $(OBJDIR)/XrdOucTrace.o

$(OBJDIR)/XrdOucXSLock.o: XrdOucError.hh XrdOucXSLock.hh XrdOucXSLock.cc
	@echo Compiling XrdOucXSLock.cc...
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucXSLock.cc -o $(OBJDIR)/XrdOucXSLock.o
