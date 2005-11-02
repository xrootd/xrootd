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
        XrdPosixLinkage.cc  \
        XrdPosixXrootd.cc

SOURCET = \
        XrdPosixPreload.cc \
        XrdPosixPreload32.cc \
        XrdPosixStream.cc

OBJECTS  = \
        $(OBJDIR)/XrdPosix.o  \
        $(OBJDIR)/XrdPosixLinkage.o  \
        $(OBJDIR)/XrdPosixXrootd.o

OBJECTT = \
        $(OBJDIR)/XrdPosixLinkage.o  \
        $(OBJDIR)/XrdPosixPreload.o  \
        $(OBJDIR)/XrdPosixPreload32.o \
        $(OBJDIR)/XrdPosixStream.o

MORELIBS = -L$(LIBDIR) -lXrdClient -lXrdNet -lXrdOuc -lXrdSec

MORELIBT = -L$(LIBDIR) -lXrdPosix

LIBDEP   = $(LIBDIR)/libXrdClient.a \
           $(LIBDIR)/libXrdNet.a \
           $(LIBDIR)/libXrdOuc.a

LIBRARY  = $(LIBDIR)/libXrdPosix.$(TYPESHLIB)

LIBPLDR  = $(LIBDIR)/libXrdPosixPreload.$(TYPESHLIB)

TARGETS  = $(LIBRARY) $(LIBPLDR)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdClient% ../XrdClient
vpath XrdOuc%    ../XrdOuc

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdPosix done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTP) $(OBJECTS) $(OBJECTT) $(LIBDEP)
	@echo Creating shared library $(LIBRARY) 
	$(ECHO)$(CC) $(CFLAGS) $(OBJECTS) $(LDSO) $(MORELIBS) $(LIBS) -o $(LIBRARY)
	@echo Creating shared library $(LIBPLDR)
	$(ECHO)$(CC) $(CFLAGS) $(OBJECTT) $(LDSO) $(MORELIBT)         -o $(LIBPLDR)

$(OBJDIR)/XrdPosix.o:  XrdPosix.cc XrdPosix.hh XrdPosixXrootd.hh  \
                       XrdPosixExtern.hh XrdPosixLinkage.hh XrdOucTokenizer.hh
	@echo Compiling XrdPosix.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPosix.o XrdPosix.cc

$(OBJDIR)/XrdPosixLinkage.o:  XrdPosixLinkage.cc   XrdPosixLinkage.hh
	@echo Compiling XrdPosixLinkage.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPosixLinkage.o XrdPosixLinkage.cc

$(OBJDIR)/XrdPosixPreload.o:  XrdPosixPreload.cc   XrdPosixLinkage.hh \
                              XrdPosixExtern.hh    XrdPosixStream.hh \
                              XrdPosixXrootd.hh
	@echo Compiling XrdPosiXPreload.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPosixPreload.o XrdPosixPreload.cc

$(OBJDIR)/XrdPosixPreload32.o:  XrdPosixPreload32.cc   XrdPosixLinkage.hh \
                                XrdPosixExtern.hh      XrdPosixStream.hh \
                                XrdPosixXrootd.hh
	@echo Compiling XrdPosiXPreload32.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPosixPreload32.o XrdPosixPreload32.cc

$(OBJDIR)/XrdPosixStream.o:  XrdPosixStream.cc   XrdPosixStream.hh \
                             XrdOucPthread.hh    XrdPosixLinkage.hh \
                             XrdPosixExtern.hh   XrdPosixXrootd.hh
	@echo Compiling XrdPosiXStream.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPosixStream.o XrdPosixStream.cc

$(OBJDIR)/XrdPosixXrootd.o:  XrdPosixXrootd.cc   XrdPosixXrootd.hh \
                             XrdClient.hh        XrdPosixExtern.hh
	@echo Compiling XrdPosixXrootd.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPosixXrootd.o XrdPosixXrootd.cc
