#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#

SOURCES  = \
        XrdOfs.cc  \
        XrdOfsConfig.cc \
        XrdOfsHandle.cc

OBJECTS  = \
        $(OBJDIR)/XrdOfs.o \
        $(OBJDIR)/XrdOfsConfig.o  \
        $(OBJDIR)/XrdOfsHandle.o  

MORELIBS = -L$(LIBDIR) -lXrdAcc -lXrdOss -lXrdOdc -lXrdXr -lXrdNet -lXrdOuc

LIBDEP   = $(LIBDIR)/libXrdAcc.a $(LIBDIR)/libXrdOss.a \
           $(LIBDIR)/libXrdOdc.a $(LIBDIR)/libXrdOuc.a \
           $(LIBDIR)/libXrdXr.a		

LIBRARY  = $(LIBDIR)/libXrdOfs.$(TYPESHLIB)

TARGETS  = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdNet% ../XrdNet
vpath XrdOdc% ../XrdOdc
vpath XrdOfs% ../XrdOfs
vpath XrdOss% ../XrdOss
vpath XrdOuc% ../XrdOuc
vpath XrdAcc% ../XrdAcc
vpath XrdSec% ../XrdSec
vpath XrdSfs% ../XrdSfs

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdOfs done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS) $(LIBDEP)
	@echo Creating shared library $(LIBRARY) 
	$(ECHO)$(CC) $(CFLAGS) $(OBJECTS) $(LDSO) $(MORELIBS) $(LIBS)  -o $(LIBRARY)

$(OBJDIR)/XrdOfs.o:  XrdOfs.cc        XrdOfs.hh \
                     XrdOfsConfig.hh  XrdOfsHandle.hh    \
                     XrdOfsOpaque.hh  XrdOfsSecurity.hh     XrdOfsTrace.hh     \
                     XrdOuca2x.hh     XrdOucEnv.hh          XrdOucError.hh     \
                     XrdOucHash.hh    XrdOucHash.icc \
                     XrdOucLock.hh    XrdOucLogger.hh    \
                     XrdOucPList.hh   XrdOucPthread.hh      XrdOucTrace.hh     \
                     XrdNetDNS.hh     XrdSecEntity.hh       XrdSfsNative.hh    \
                     XrdSfsAio.hh     XrdOss.hh          \
                     XrdOucTList.hh   XrdOdcFinder.hh  XrdAccAuthorize.hh      \
                  ../XrdVersion.hh
	@echo Compiling XrdOfs.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOfs.o XrdOfs.cc

$(OBJDIR)/XrdOfsConfig.o:  XrdOfsConfig.cc   XrdOfsConfig.hh \
                           XrdOfs.hh         XrdOfsTrace.hh  \
                           XrdOuca2x.hh      XrdOucError.hh   XrdOucStream.hh \
                           XrdOucTrace.hh    XrdOucUtils.hh   XrdNetDNS.hh   \
                           XrdAccAuthorize.hh \
                           XrdOdcFinder.hh
	@echo Compiling XrdOfsConfig.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOfsConfig.o XrdOfsConfig.cc

$(OBJDIR)/XrdOfsHandle.o:  XrdOfsHandle.cc   XrdOfsHandle.hh \
                           XrdOfs.hh         XrdOfsConfig.hh  XrdOfsTrace.hh  \
                           XrdOuca2x.hh      XrdOucError.hh   XrdOucStream.hh \
                           XrdOucTrace.hh                                     \
                           XrdOdcFinder.hh
	@echo Compiling XrdOfsHandle.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOfsHandle.o XrdOfsHandle.cc
