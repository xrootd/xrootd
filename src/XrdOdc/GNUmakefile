#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

BINLIBS = -L$(LIBDIR) -lXrdOdc -l XrdNet -lXrdOuc

LIBDEPS = $(LIBDIR)/libXrdOdc.a $(LIBDIR)/libXrdNet.a $(LIBDIR)/libXrdOuc.a

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdOdcConfig.cc \
        XrdOdcFinder.cc \
        XrdOdcManager.cc \
        XrdOdcMsg.cc \
        XrdOdcTest.cc
  
OBJECTA = \
        $(OBJDIR)/XrdOdcConfig.o \
        $(OBJDIR)/XrdOdcFinder.o \
        $(OBJDIR)/XrdOdcManager.o \
        $(OBJDIR)/XrdOdcMsg.o

OBJECTB = \
        $(OBJDIR)/XrdOdcTest.o

OBJECTS = $(OBJECTA) $(OBJECTB)

LIBRARY = $(LIBDIR)/libXrdOdc.a

# TESTBIN = $(BINDIR)/test_odc

TARGETS = $(LIBRARY) $(TESTBIN)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdNet% ../XrdNet
vpath XrdOdc% ../XrdOdc
vpath XrdOuc% ../XrdOuc
vpath XrdSfs% ../XrdSfs

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdOdc done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(LIBRARY): $(OBJECTA)
	@echo Creating archive $(LIBRARY) 
	$(ECHO)rm -f $(LIBRARY)
	$(ECHO)ar -rc $(LIBRARY) $(OBJECTA)
	$(ECHO)if [ "x$(SUNCACHE)" != "x" ]; then \
	   if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	      ar -rc $(LIBRARY) $(OBJDIR)$(SUNCACHE)/*/*.o; \
	   fi;\
	fi

$(TESTBIN): $(OBJECTB) $(LIBDEPS)
	@echo Creating executable $(TESTBIN) 
	$(ECHO)$(LD) $(LDOP) $(OBJECTB) $(BINLIBS) $(LIBS) -o $(TESTBIN)

$(OBJDIR)/XrdOdcConfig.o: XrdOdcConfig.hh XrdOdcMsg.hh     XrdOdcTrace.hh \
                          XrdOuca2x.hh    XrdOucStream.hh  XrdOucTList.hh  \
                          XrdNetWork.hh \
                          XrdOdcConfig.cc XrdOdcConfDefs.hh
	@echo Compiling XrdOdcConfig.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcConfig.o XrdOdcConfig.cc

$(OBJDIR)/XrdOdcFinder.o: XrdOdcFinder.cc   XrdOdcFinder.hh XrdOdcManager.hh \
                          XrdOdcMsg.hh      XrdOdcTrace.hh  \
                          XrdOucPthread.hh  XrdOdcConfDefs.hh XrdOucError.hh \
                          XrdOucErrInfo.hh  XrdOucPlatform.hh XrdOucPthread.hh \
                          XrdOucReqID.hh    XrdOucStream.hh   XrdOucTimer.hh \
                          XrdNetDNS.hh      XrdNetOpts.hh     XrdNetSocket.hh \
                          XrdSfsInterface.hh
	@echo Compiling XrdOdcFinder.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcFinder.o XrdOdcFinder.cc

$(OBJDIR)/XrdOdcManager.o: XrdOdcManager.hh  XrdOdcManager.cc \
                           XrdOdcMsg.hh      XrdOdcTrace.hh \
                           XrdOucError.hh    XrdOucPthread.hh \
                           XrdNetLink.hh     XrdNetWork.hh
	@echo Compiling XrdOdcManager.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcManager.o XrdOdcManager.cc

$(OBJDIR)/XrdOdcMsg.o: XrdOdcMsg.hh     XrdOdcTrace.hh  XrdOucErrInfo.hh \
                       XrdOucPthread.hh \
                       XrdOdcMsg.cc
	@echo Compiling XrdOdcMsg.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcMsg.o XrdOdcMsg.cc

$(OBJDIR)/XrdOdcTest.o: XrdOucError.hh   XrdOdcFinder.hh  XrdOucErrInfo.hh \
                        XrdOucLogger.hh  XrdOucStream.hh   \
                        XrdOdcTest.cc
	@echo Compiling XrdOdcTest.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcTest.o XrdOdcTest.cc
