#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
include ../GNUmake.env

BINLIBS = -L$(LIBDIR) -lXrdOuc -lXrdSec


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
          $(LIBDIR)/libXrdOuc.a 

LIBRARY = $(LIBDIR)/libXrdXr.a

TESTBIN = $(BINDIR)/xrclient$(BINSFX)

TARGETS = $(LIBRARY) $(TESTBIN)


#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                           -
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc
vpath XrdSec% ../XrdSec
vpath Xrd%    ../Xrd
vpath XP%     ../XProtocol
vpath XrdXr%  ../XrdXr


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
	@echo Creating shared library $(LIBRARY)
	$(ECHO)ar r $(LIBRARY) $(XROBJS)

$(OBJDIR)/XrdXrClientWorker.o: XrdXrClientWorker.hh XrdXrClientWorker.cc
	@echo Compiling XrdXrClientWorker.cc ...
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrClientWorker.o \
	XrdXrClientWorker.cc

$(OBJDIR)/XrdXrClient.o:  XrdXrClient.hh XrdXrClient.cc
	@echo Compiling XrdXrClient.cc ...
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrClient.o XrdXrClient.cc

$(OBJDIR)/XrdXrMain.o: XrdXrClient.hh XrdXrClientWorker.hh \
	XrdXrClientWorker.cc XrdXrMain.cc
	@echo Compiling XrdXrMain.cc ...
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrMain.o XrdXrMain.cc
