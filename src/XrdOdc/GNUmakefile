#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

BINLIBS = -L$(LIBDIR) -lXrdOdc -lXrdOuc

LIBDEPS = $(LIBDIR)/libXrdOdc.a $(LIBDIR)/libXrdOuc.a

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

TESTBIN = $(BINDIR)/test_odc

TARGETS = $(LIBRARY) $(TESTBIN)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdOdc% ../XrdOdc
vpath XrdOuc% ../XrdOuc
vpath XrdSfs% ../XrdSfs

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.rules

anything: $(TARGETS)
	@echo Make XrdOdc done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(LIBRARY): $(OBJECTA)
	@echo Creating archive $(LIBRARY) 
	@rm -f $(LIBRARY)
	@ar -rc $(LIBRARY) $(OBJECTA)
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	 ar -rc $(LIBRARY) $(OBJDIR)/SunWS_cache/*/*.o; \
fi

$(TESTBIN): $(OBJECTB) $(LIBDEPS)
	@echo Creating executable $(TESTBIN) 
	@$(CC) $(OBJECTB) $(BINLIBS) $(LIBS) -o $(TESTBIN)

$(OBJDIR)/XrdOdcConfig.o: XrdOdcConfig.hh XrdOdcMsg.hh     XrdOdcTrace.hh \
                          XrdOuca2x.hh    XrdOucNetwork.hh XrdOucStream.hh \
                          XrdOucTList.hh  \
                          XrdOdcConfig.cc XrdOdcConfDefs.hh
	@echo Compiling XrdOdcConfig.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcConfig.o XrdOdcConfig.cc

$(OBJDIR)/XrdOdcFinder.o: XrdOdcFinder.cc   XrdOdcFinder.hh XrdOdcManager.hh \
                          XrdOdcMsg.hh      XrdOdcTrace.hh  XrdOucNetwork.hh \
                          XrdOucPthread.hh  XrdOdcConfDefs.hh XrdOucError.hh \
                          XrdOucErrInfo.hh  XrdOucPlatform.hh XrdOucTimer.hh \
                          XrdOucReqID.hh    XrdOucSocket.hh   XrdOucStream.hh \
                          XrdSfsInterface.hh
	@echo Compiling XrdOdcFinder.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcFinder.o XrdOdcFinder.cc

$(OBJDIR)/XrdOdcManager.o: XrdOdcManager.hh  XrdOdcMsg.hh  XrdOdcTrace.hh \
                           XrdOucError.hh    XrdOucLink.hh XrdOucNetwork.hh \
                           XrdOucPthread.hh \
                           XrdOdcManager.cc
	@echo Compiling XrdOdcManager.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcManager.o XrdOdcManager.cc

$(OBJDIR)/XrdOdcMsg.o: XrdOdcMsg.hh     XrdOdcTrace.hh  XrdOucErrInfo.hh \
                       XrdOucPthread.hh \
                       XrdOdcMsg.cc
	@echo Compiling XrdOdcMsg.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcMsg.o XrdOdcMsg.cc

$(OBJDIR)/XrdOdcTest.o: XrdOucError.hh   XrdOdcFinder.hh  XrdOucErrInfo.hh \
                        XrdOucLogger.hh  XrdOucStream.hh   \
                        XrdOdcTest.cc
	@echo Compiling XrdOdcTest.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOdcTest.o XrdOdcTest.cc
