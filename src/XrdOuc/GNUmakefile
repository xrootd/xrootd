#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdOuca2x.cc         \
        XrdOucEnv.cc         \
        XrdOucError.cc       \
        XrdOucHashVal.cc     \
        XrdOucLogger.cc      \
        XrdOucNList.cc       \
        XrdOucPlatform.cc    \
        XrdOucProg.cc        \
        XrdOucPthread.cc     \
        XrdOucReqID.cc       \
        XrdOucStream.cc      \
        XrdOucString.cc      \
        XrdOucTimer.cc       \
        XrdOucTokenizer.cc   \
        XrdOucTrace.cc       \
        XrdOucUtils.cc       \
        XrdOucXSLock.cc

  
OBJECTS = \
        $(OBJDIR)/XrdOuca2x.o          \
        $(OBJDIR)/XrdOucError.o        \
        $(OBJDIR)/XrdOucHashVal.o      \
        $(OBJDIR)/XrdOucEnv.o          \
        $(OBJDIR)/XrdOucLogger.o       \
        $(OBJDIR)/XrdOucNList.o        \
        $(OBJDIR)/XrdOucPlatform.o     \
        $(OBJDIR)/XrdOucProg.o         \
        $(OBJDIR)/XrdOucPthread.o      \
        $(OBJDIR)/XrdOucReqID.o        \
        $(OBJDIR)/XrdOucStream.o       \
        $(OBJDIR)/XrdOucString.o       \
        $(OBJDIR)/XrdOucTimer.o        \
        $(OBJDIR)/XrdOucTokenizer.o    \
        $(OBJDIR)/XrdOucTrace.o        \
        $(OBJDIR)/XrdOucUtils.o        \
        $(OBJDIR)/XrdOucXSLock.o

LIBRARY = $(LIBDIR)/libXrdOuc.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdNet% ../XrdNet

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdOuc done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS)
	@echo Creating archive $(LIBRARY) 
	$(ECHO)rm -f $(LIBRARY)
	$(ECHO)if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	$(CC) -xar -o $(LIBRARY) $(OBJECTS); \
	else \
	ar -rc $(LIBRARY) $(OBJECTS); \
	ranlib $(LIBRARY); \
fi

$(OBJDIR)/XrdOuca2x.o: XrdOuca2x.cc XrdOuca2x.hh XrdOucError.hh
	@echo Compiling XrdOuca2x.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOuca2x.cc -o $(OBJDIR)/XrdOuca2x.o

$(OBJDIR)/XrdOucEnv.o: XrdOucEnv.cc XrdOucEnv.hh XrdOucHash.hh
	@echo Compiling XrdOucEnv.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucEnv.cc -o $(OBJDIR)/XrdOucEnv.o

$(OBJDIR)/XrdOucError.o: XrdOucError.cc XrdOucError.hh XrdOucLogger.hh \
                         XrdOucPlatform.hh
	@echo Compiling XrdOucError.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucError.cc -o $(OBJDIR)/XrdOucError.o

$(OBJDIR)/XrdOucHashVal.o: XrdOucHashVal.cc
	@echo Compiling XrdOucHashVal.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucHashVal.cc -o $(OBJDIR)/XrdOucHashVal.o

$(OBJDIR)/XrdOucLogger.o: XrdOucLogger.cc XrdOucLogger.hh \
                          XrdOucTimer.hh  XrdOucPthread.hh
	@echo Compiling XrdOucLogger.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucLogger.cc -o $(OBJDIR)/XrdOucLogger.o

$(OBJDIR)/XrdOucNList.o: XrdOucNList.cc XrdOucNList.hh XrdOucPthread.hh
	@echo Compiling XrdOucNList.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucNList.cc -o $(OBJDIR)/XrdOucNList.o

$(OBJDIR)/XrdOucPlatform.o: XrdOucPlatform.cc XrdOucPlatform.hh
	@echo Compiling XrdOucPlatform.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(CFSTRLCPY) $(INCLUDE) XrdOucPlatform.cc -o $(OBJDIR)/XrdOucPlatform.o

$(OBJDIR)/XrdOucProg.o: XrdOucProg.cc  XrdOucProg.hh \
                        XrdOucError.hh XrdOucStream.hh XrdOucPthread.hh
	@echo Compiling XrdOucProg.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucProg.cc -o $(OBJDIR)/XrdOucProg.o

$(OBJDIR)/XrdOucPthread.o: XrdOucPthread.cc XrdOucPthread.hh XrdOucError.hh
	@echo Compiling XrdOucPthread.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucPthread.cc -o $(OBJDIR)/XrdOucPthread.o

$(OBJDIR)/XrdOucReqID.o: XrdOucReqID.cc XrdOucReqID.hh XrdOucPthread.hh
	@echo Compiling XrdOucReqID.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucReqID.cc -o $(OBJDIR)/XrdOucReqID.o

$(OBJDIR)/XrdOucStream.o: XrdOucStream.cc XrdOucStream.hh XrdOucError.hh \
                          XrdOucUtils.hh
	@echo Compiling XrdOucStream.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucStream.cc -o $(OBJDIR)/XrdOucStream.o

$(OBJDIR)/XrdOucString.o: XrdOucString.cc XrdOucString.hh
	@echo Compiling XrdOucString.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucString.cc -o $(OBJDIR)/XrdOucString.o

$(OBJDIR)/XrdOucTimer.o: XrdOucTimer.cc XrdOucTimer.hh  
	@echo Compiling XrdOucTimer.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucTimer.cc -o $(OBJDIR)/XrdOucTimer.o

$(OBJDIR)/XrdOucTokenizer.o: XrdOucTokenizer.cc XrdOucTokenizer.hh  
	@echo Compiling XrdOucTokenizer.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucTokenizer.cc -o $(OBJDIR)/XrdOucTokenizer.o

$(OBJDIR)/XrdOucTrace.o: XrdOucError.hh XrdOucTrace.hh XrdOucTrace.cc
	@echo Compiling XrdOucTrace.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucTrace.cc -o $(OBJDIR)/XrdOucTrace.o

$(OBJDIR)/XrdOucUtils.o: XrdOucUtils.cc XrdOucUtils.hh \
                      XrdOucError.hh XrdOucStream.hh XrdNetDNS.hh
	@echo Compiling XrdOucUtils.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucUtils.cc -o $(OBJDIR)/XrdOucUtils.o

$(OBJDIR)/XrdOucXSLock.o: XrdOucError.hh XrdOucXSLock.hh XrdOucXSLock.cc
	@echo Compiling XrdOucXSLock.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucXSLock.cc -o $(OBJDIR)/XrdOucXSLock.o
