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
        XrdOucCRC.cc         \
        XrdOucEnv.cc         \
        XrdOucError.cc       \
        XrdOucExport.cc      \
        XrdOucHashVal.cc     \
        XrdOucLogger.cc      \
        XrdOucMsubs.cc       \
        XrdOucName2Name.cc   \
        XrdOucNList.cc       \
        XrdOucReqID.cc       \
        XrdOucStream.cc      \
        XrdOucString.cc      \
        XrdOucTokenizer.cc   \
        XrdOucTrace.cc       \
        XrdOucUtils.cc

OBJECTS = \
        $(OBJDIR)/XrdOuca2x.o          \
        $(OBJDIR)/XrdOucCRC.o          \
        $(OBJDIR)/XrdOucEnv.o          \
        $(OBJDIR)/XrdOucError.o        \
        $(OBJDIR)/XrdOucExport.o       \
        $(OBJDIR)/XrdOucHashVal.o      \
        $(OBJDIR)/XrdOucLogger.o       \
        $(OBJDIR)/XrdOucMsubs.o        \
        $(OBJDIR)/XrdOucName2Name.o    \
        $(OBJDIR)/XrdOucNList.o        \
        $(OBJDIR)/XrdOucReqID.o        \
        $(OBJDIR)/XrdOucStream.o       \
        $(OBJDIR)/XrdOucString.o       \
        $(OBJDIR)/XrdOucTokenizer.o    \
        $(OBJDIR)/XrdOucTrace.o        \
        $(OBJDIR)/XrdOucUtils.o

LIBRARY = $(LIBDIR)/libXrdOuc.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdNet% ../XrdNet
vpath XrdSys% ../XrdSys

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

$(OBJDIR)/XrdOucCRC.o: XrdOucCRC.cc XrdOucCRC.hh
	@echo Compiling XrdOucCRC.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucCRC.cc -o $(OBJDIR)/XrdOucCRC.o

$(OBJDIR)/XrdOucEnv.o: XrdOucEnv.cc XrdOucEnv.hh XrdOucHash.hh
	@echo Compiling XrdOucEnv.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucEnv.cc -o $(OBJDIR)/XrdOucEnv.o

$(OBJDIR)/XrdOucError.o: XrdOucError.cc XrdOucError.hh XrdOucLogger.hh \
                         XrdSysPlatform.hh
	@echo Compiling XrdOucError.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucError.cc -o $(OBJDIR)/XrdOucError.o

$(OBJDIR)/XrdOucExport.o: XrdOucExport.cc XrdOucExport.hh \
                          XrdOucStream.hh XrdOucPList.hh  XrdOucError.hh \
                          XrdSysPlatform.hh
	@echo Compiling XrdOucExport.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucExport.cc -o $(OBJDIR)/XrdOucExport.o

$(OBJDIR)/XrdOucHashVal.o: XrdOucHashVal.cc
	@echo Compiling XrdOucHashVal.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucHashVal.cc -o $(OBJDIR)/XrdOucHashVal.o

$(OBJDIR)/XrdOucLogger.o: XrdOucLogger.cc XrdOucLogger.hh \
                          XrdSysTimer.hh  XrdSysPthread.hh
	@echo Compiling XrdOucLogger.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucLogger.cc -o $(OBJDIR)/XrdOucLogger.o

$(OBJDIR)/XrdOucMsubs.o: XrdOucMsubs.cc XrdOucMsubs.hh \
                         XrdOucEnv.hh   XrdOucName2Name.hh
	@echo Compiling XrdOucMsubs.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucMsubs.cc -o $(OBJDIR)/XrdOucMsubs.o

$(OBJDIR)/XrdOucName2Name.o: XrdOucName2Name.cc XrdOucName2Name.hh \
                             XrdOucError.hh XrdSysPlatform.hh
	@echo Compiling XrdOucName2Name.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucName2Name.cc -o $(OBJDIR)/XrdOucName2Name.o

$(OBJDIR)/XrdOucNList.o: XrdOucNList.cc XrdOucNList.hh XrdSysPthread.hh
	@echo Compiling XrdOucNList.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucNList.cc -o $(OBJDIR)/XrdOucNList.o

$(OBJDIR)/XrdOucReqID.o: XrdOucReqID.cc XrdOucReqID.hh XrdSysPthread.hh
	@echo Compiling XrdOucReqID.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucReqID.cc -o $(OBJDIR)/XrdOucReqID.o

$(OBJDIR)/XrdOucStream.o: XrdOucStream.cc XrdOucStream.hh XrdOucError.hh \
                          XrdOucUtils.hh  XrdSysPlatform.hh
	@echo Compiling XrdOucStream.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucStream.cc -o $(OBJDIR)/XrdOucStream.o

$(OBJDIR)/XrdOucString.o: XrdOucString.cc XrdOucString.hh
	@echo Compiling XrdOucString.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdOucString.cc -o $(OBJDIR)/XrdOucString.o

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
