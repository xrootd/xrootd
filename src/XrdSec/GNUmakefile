#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

LIBPATH = -L$(LIBDIR) -lXrdOuc

LIBDEPS = $(LIBDIR)/libXrdOuc.a

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdSecClient.cc        \
        XrdSecPManager.cc      \
        XrdSecProtocolsrvr.cc  \
        XrdSectestClient.cc    \
        XrdSectestServer.cc    \
  
OBJSLIB = \
        $(OBJDIR)/XrdSecClient.o        \
        $(OBJDIR)/XrdSecPManager.o      \
        $(OBJDIR)/XrdSecProtocolsrvr.o

OBJECTA = \
        $(OBJDIR)/XrdSectestClient.o

OBJECTB = \
        $(OBJDIR)/XrdSectestServer.o

OBJECTS = $(OBJSLIB) $(OBJECTA) $(OBJECTB)

LIBRARY = $(LIBDIR)/libXrdSec.so

TSTBINA = $(BINDIR)/testclient
TSTBINB = $(BINDIR)/testserver

TARGETS = $(LIBRARY) $(TSTBINA) $(TSTBINB)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdSec done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(LIBRARY): $(OBJSLIB) $(LIBDEPS)
	@echo Creating shared library $(LIBRARY) 
	$(ECHO)$(CC) $(OBJSLIB) $(LDSO) $(LIBPATH) $(LIBS) -o $(LIBRARY)

$(TSTBINA): $(OBJECTA) $(LIBDEPS)
	@echo Creating executable $(TSTBINA) 
	$(ECHO)$(LD) $(LDOP) $(OBJECTA) $(BINLIBS) $(LIBPATH) -lXrdSec $(LIBS) -o $(TSTBINA)

$(TSTBINB): $(OBJECTB) $(LIBDEPS)
	@echo Creating executable $(TSTBINB) 
	$(ECHO)$(LD) $(LDOP) $(OBJECTB) $(BINLIBS) $(LIBPATH) -lXrdSec $(LIBS) -o $(TSTBINB)

$(OBJDIR)/XrdSecClient.o: XrdOucPthread.hh XrdSecPManager.hh XrdSecInterface.hh \
                          XrdOucErrInfo.hh XrdSecClient.cc
	@echo Compiling XrdSecClient.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSecClient.o XrdSecClient.cc

$(OBJDIR)/XrdSecPManager.o: XrdSecInterface.hh XrdSecPManager.hh \
                            XrdSecProtocolhost.hh \
                            XrdOucErrInfo.hh   XrdOucPthread.hh \
                            XrdSecPManager.cc
	@echo Compiling XrdSecPManager.cc 
	$(ECHO)$(CC) -c $(CFLAGS32) $(INCLUDE) -o $(OBJDIR)/XrdSecPManager.o XrdSecPManager.cc

$(OBJDIR)/XrdSecProtocolsrvr.o: XrdSecProtocolsrvr.cc XrdSecProtocolsrvr.hh \
                                XrdOucErrInfo.hh      XrdOucError.hh \
                                XrdOucLogger.hh       XrdOucStream.hh \
                                XrdSecInterface.hh    XrdSecPManager.hh \
                                XrdSecTrace.hh
	@echo Compiling XrdSecProtocolsrvr.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSecProtocolsrvr.o XrdSecProtocolsrvr.cc

$(OBJDIR)/XrdSectestClient.o:   XrdSectestClient.cc XrdSecInterface.hh
	@echo Compiling XrdSectestClient.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSectestClient.o XrdSectestClient.cc

$(OBJDIR)/XrdSectestServer.o: XrdSecProtocolsrvr.hh XrdSecInterface.hh XrdOucLogger.hh \
                              XrdSectestServer.cc
	@echo Compiling XrdSectestServer.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdSectestServer.o XrdSectestServer.cc
