#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#

SOURCES  = \
        XrdPosix.cc  \
        XrdPosixXrootd.cc

OBJECTS  = \
        $(OBJDIR)/XrdPosix.o  \
        $(OBJDIR)/XrdPosixXrootd.o

MORELIBS = -L$(LIBDIR) -lXrdClient -lXrdNet -lXrdOuc -lXrdSec

LIBDEP   = $(LIBDIR)/libXrdClient.a \
           $(LIBDIR)/libXrdNet.a \
           $(LIBDIR)/libXrdOuc.a

LIBRARY  = $(LIBDIR)/libXrdPosix.$(TYPESHLIB)

TARGETS  = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdClient% ../XrdClient

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdPosix done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTP) $(OBJECTS) $(LIBDEP)
	@echo Creating shared library $(LIBRARY) 
	$(ECHO)$(CC) $(CFLAGS) $(OBJECTS) $(LDSO) $(MORELIBS) $(LIBS) -o $(LIBRARY)

$(OBJDIR)/XrdPosix.o:  XrdPosix.cc XrdPosix.hh XrdPosixXrootd.hh
	@echo Compiling XrdPosix.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPosix.o XrdPosix.cc

$(OBJDIR)/XrdPosixXrootd.o:  XrdPosixXrootd.cc   XrdPosixXrootd.hh \
                             XrdClient.hh
	@echo Compiling XrdPosixXrootd.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPosixXrootd.o XrdPosixXrootd.cc
