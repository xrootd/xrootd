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
        XrdBuffer.cc      \
        XrdConfig.cc      \
        XrdLink.cc        \
        XrdMain.cc        \
        XrdNetwork.cc     \
        XrdPoll.cc        \
        XrdProtocol.cc    \
        XrdScheduler.cc   \
        XrdStats.cc       \
        XrdInfo.cc

  
OBJECTS = \
        $(OBJDIR)/XrdBuffer.o      \
        $(OBJDIR)/XrdConfig.o      \
        $(OBJDIR)/XrdLink.o        \
        $(OBJDIR)/XrdMain.o        \
        $(OBJDIR)/XrdNetwork.o     \
        $(OBJDIR)/XrdPoll.o        \
        $(OBJDIR)/XrdProtocol.o    \
        $(OBJDIR)/XrdScheduler.o   \
        $(OBJDIR)/XrdStats.o       \
        $(OBJDIR)/XrdInfo.o

LIBRARY = $(LIBDIR)/libXrd.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                           -
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#        A r c h i t e c t u r e   S p e c i f i c   V a r i a b l e s         #
#------------------------------------------------------------------------------#

CFLAGS     =  $(CF64) $(CF32) $(CFOP)
  
SUN64      = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SUNMT      = -D_REENTRANT -mt -D_POSIX_PTHREAD_SEMANTICS
SUNOPT     = -fast -xarch=v8plus -D_DEVPOLL
SUNDBG     = -g -D_DEVPOLL
SUN32      = -DSUN $(SUNMT) $(SUNCCT)

LNX64      = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LNXOPT     = -O3
LNXDBG     = -g
LNX32      = -D__linux__ -D_ALL_SOURCE -D_REENTRANT -D_GNU_SOURCE \
             -Wno-deprecated

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
all:
	@/bin/mkdir -p $(LIBDIR) $(OBJDIR)
	@$(MAKE) $(TYPE)all --no-print-directory

debug:
	@/bin/mkdir -p $(LIBDIR)/dbg $(OBJDIR)/dbg
	@$(MAKE) $(TYPE)dbg DBGSFX=/dbg --no-print-directory

Linuxall:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++    \
CF64="$(LNX64)" \
CF32="$(LNX32)" \
CFOP="$(LNXOPT)" \
TYPE=Linux

Linuxdbg:
	@$(MAKE) anything --no-print-directory \
ARCH=$(ARCH) \
CC=g++    \
CF64="$(LNX64)" \
CF32="$(LNX32)" \
CFOP="$(LNXDBG)" \
TYPE=Linux

SunOSall:
	@$(MAKE) anything \
ARCH=$(ARCH) \
CC=CC \
CF64="$(SUN64)" \
CF32="$(SUN32)" \
CFOP="$(SUNOPT)" \
TYPE=SunOS

SunOSdbg:
	@$(MAKE) anything \
ARCH=$(ARCH) \
CC=CC \
CF64="$(SUN64)" \
CF32="$(SUN32)" \
CFOP="$(SUNDBG)" \
TYPE=SunOS

anything: $(TARGETS)
	@echo Make xrd done.

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
	@rm -f $(LIBRARY);
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	$(CC) -xar -o $(LIBRARY) $(OBJECTS); \
else \
	ar -rc $(LIBRARY) $(OBJECTS); \
fi

$(OBJDIR)/XrdBuffer.o:    XrdBuffer.cc   XrdBuffer.hh     XrdTrace.hh \
                          XrdOucError.hh XrdOucPthread.hh XrdOucTrace.hh \
                          XrdOucTimer.hh
	@echo Compiling XrdBuffer.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdBuffer.o XrdBuffer.cc

$(OBJDIR)/XrdConfig.o: XrdBuffer.hh      XrdConfig.hh      XrdJob.hh     \
                       XrdLink.hh        XrdNetwork.hh     XrdObject.hh  \
                       XrdPoll.hh        XrdProtocol.hh    XrdScheduler.hh \
                       XrdStats.hh       XrdTrace.hh       XrdInfo.hh    \
                       XrdOuca2x.hh      XrdOucError.hh    XrdOucLogger.hh   \
                       XrdOucSecurity.hh XrdOucStream.hh   XrdOucTrace.hh \
                       XrdOucTimer.hh    XrdConfig.cc
	@echo Compiling XrdConfig.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdConfig.o XrdConfig.cc

$(OBJDIR)/XrdLink.o:   XrdBuffer.hh     XrdJob.hh          XrdLink.hh      \
                       XrdNetwork.hh    XrdObject.hh       XrdObject.icc  \
                       XrdProtocol.hh   XrdScheduler.hh    XrdTrace.hh  \
                       XrdOucError.hh   XrdOucPlatform.hh  XrdOucPthread.hh \
                       XrdOucTrace.hh   XrdOucTimer.hh     XrdLink.cc
	@echo Compiling XrdLink.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdLink.o XrdLink.cc

$(OBJDIR)/XrdMain.o:   XrdConfig.hh    XrdLink.hh        XrdNetwork.hh  \
                       XrdPoll.hh      XrdScheduler.hh   XrdTrace.hh    \
                       XrdOucError.hh  XrdOucLogger.hh   XrdOucPthread.hh \
                       XrdOucTrace.hh    \
                       XrdMain.cc
	@echo Compiling XrdMain.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdMain.o XrdMain.cc

$(OBJDIR)/XrdNetwork.o: XrdBuffer.hh       XrdLink.hh       XrdNetwork.hh   \
                        XrdTrace.hh  \
                        XrdOucError.hh     XrdOucLogger.hh  XrdOucPlatform.hh \
                        XrdOucPthread.hh \
                        XrdOucSecurity.hh  XrdOucTrace.hh   XrdNetwork.cc
	@echo Compiling XrdNetwork.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdNetwork.o XrdNetwork.cc

$(OBJDIR)/XrdPoll.o:   XrdLink.hh       XrdPoll.hh       XrdProtocol.hh   \
                       XrdPoll.cc       XrdPollDev.hh    XrdPollDev.icc \
                       XrdPollPoll.hh   XrdPollDev.icc   XrdTrace.hh     \
                       XrdOucError.hh   XrdOucPthread.hh XrdOucTrace.hh
	@echo Compiling XrdPoll.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPoll.o XrdPoll.cc

$(OBJDIR)/XrdProtocol.o: XrdJob.hh        XrdLink.hh     XrdPoll.hh \
                         XrdProtocol.cc   XrdProtocol.hh XrdTrace.hh \
                         XrdOucError.hh   XrdOucTrace.hh
	@echo Compiling XrdProtocol.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdProtocol.o XrdProtocol.cc

$(OBJDIR)/XrdScheduler.o: XrdJob.hh      XrdScheduler.hh   XrdTrace.hh \
                          XrdOucError.hh XrdOucPthread.hh  XrdOucTrace.hh \
                          XrdScheduler.cc
	@echo Compiling XrdScheduler.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdScheduler.o XrdScheduler.cc

$(OBJDIR)/XrdStats.o: XrdBuffer.hh     XrdLink.hh     XrdPoll.hh \
                      XrdProtocol.hh   XrdStats.cc    XrdStats.hh \
                      XrdOucPthread.hh
	@echo Compiling XrdStats.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdStats.o XrdStats.cc

$(OBJDIR)/XrdInfo.o: XrdInfo.hh
	@echo Compiling XrdInfo.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdInfo.o XrdInfo.cc
