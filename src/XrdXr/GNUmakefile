#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
include ../GNUmake.env

BINLIBS = -L../../lib/$(ARCH)$(DBGSFX) -lXrdOuc -lXrdSec


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
          ../../lib/$(ARCH)$(DBGSFX)/libXrdOuc.a 

LIBRARY = $(LIBDIR)/libXrdXr.a

TARGET  = $(BINDIR)/xrclient$(BINSFX)


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
 
include ../GNUmake.rules

anything: $(TARGET)
	@echo Make XrdXr done.


#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGET): $(OBJECTS) $(OBJDEPS)
	@echo Creating executable $(TARGET) ...
	@$(CC) $(LFLAGS) $(OBJECTS) $(LIBS) $(BINLIBS) -o $(TARGET)
	@echo Creating shared library $(LIBRARY)
	@ar r $(LIBRARY) $(XROBJS)

$(OBJDIR)/XrdXrClientWorker.o: XrdXrClientWorker.hh XrdXrClientWorker.cc
	@echo Compiling XrdXrClientWorker.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrClientWorker.o \
	XrdXrClientWorker.cc

$(OBJDIR)/XrdXrClient.o:  XrdXrClient.hh XrdXrClient.cc
	@echo Compiling XrdXrClient.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrClient.o XrdXrClient.cc

$(OBJDIR)/XrdXrMain.o: XrdXrClient.hh XrdXrClientWorker.hh \
	XrdXrClientWorker.cc XrdXrMain.cc
	@echo Compiling XrdXrMain.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdXrMain.o XrdXrMain.cc
