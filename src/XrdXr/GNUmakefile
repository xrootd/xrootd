#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
include ../GNUmake.env

BINLIBS = -L$(LIBDIR) -lXrdNet -lXrdOuc -lXrdSec

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#

SOURCES = \
	XrdClientWorker.cc \
	XrdClient.cc \
        XrdXrMain.cc 

XROBJS = \
	$(OBJDIR)/XrdXrClientWorker.o \
	$(OBJDIR)/XrdXrClient.o 	

OBJECTS = \
	$(XROBJS) \
        $(OBJDIR)/XrdXrMain.o  

OBJDEPS = \
          $(LIBDIR)/libXrdNet.a \
          $(LIBDIR)/libXrdOuc.a

LIBRARY = $(LIBDIR)/libXrdXr.a

TESTBIN = $(BINDIR)/xrclient$(BINSFX)

TARGETS = $(LIBRARY) $(TESTBIN)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                           -
#------------------------------------------------------------------------------#

vpath XrdNet%     ../XrdNet
vpath XrdOuc%     ../XrdOuc
vpath XrdSec%     ../XrdSec
vpath XrdXrootd%  ../XrdXrootd
vpath Xrd%        ../Xrd
vpath XP%         ../XProtocol


#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdXr done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS) $(OBJDEPS)
	@echo Creating executable $(TESTBIN) ...
	$(ECHO)$(LD) $(LDOP) $(LFLAGS) $(OBJECTS) $(LIBS) $(BINLIBS) -o $(TESTBIN)
	@echo Creating archive $(LIBRARY)
	$(ECHO)ar rc $(LIBRARY) $(XROBJS)

$(OBJDIR)/XrdXrClientWorker.o: XrdXrClientWorker.hh XrdXrClientWorker.cc \
                               XrdXrTrace.hh \
                               XrdNetLink.hh         XrdNetWork.hh \
                               XrdOucErrInfo.hh      XrdOucError.hh \
                               XrdOucLogger.hh  \
                               XrdXrootdProtocol.hh  XProtocol.hh
	@echo Compiling XrdXrClientWorker.cc ...
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrClientWorker.o \
	XrdXrClientWorker.cc

$(OBJDIR)/XrdXrClient.o:  XrdXrClient.hh        XrdXrClient.cc  \
                          XrdXrClientWorker.hh  XrdXrTrace.hh   \
                          XrdOucLogger.hh       XrdOucPlatform.hh \
                          XrdOucPthread.hh \
                          XrdXrootdProtocol.hh  XProtocol.hh
	@echo Compiling XrdXrClient.cc ...
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrClient.o XrdXrClient.cc

$(OBJDIR)/XrdXrMain.o: XrdXrClient.hh        XrdXrClientWorker.hh \
                       XrdXrClientWorker.cc  XrdXrMain.cc  \
                       XrdXrTrace.hh   \
                       XrdOucLogger.hh       XrdSecInterface.hh \
                       XProtocol.hh
	@echo Compiling XrdXrMain.cc ...
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrMain.o XrdXrMain.cc
