#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdNet.cc            \
        XrdNetBuffer.cc      \
        XrdNetConnect.cc     \
        XrdNetDNS.cc         \
        XrdNetLink.cc        \
        XrdNetSecurity.cc    \
        XrdNetSocket.cc      \
        XrdNetWork.cc
  
OBJECTS = \
        $(OBJDIR)/XrdNet.o             \
        $(OBJDIR)/XrdNetBuffer.o       \
        $(OBJDIR)/XrdNetConnect.o      \
        $(OBJDIR)/XrdNetDNS.o          \
        $(OBJDIR)/XrdNetLink.o         \
        $(OBJDIR)/XrdNetSecurity.o     \
        $(OBJDIR)/XrdNetSocket.o       \
        $(OBJDIR)/XrdNetWork.o

LIBRARY = $(LIBDIR)/libXrdNet.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc
vpath XrdNet% ../XrdNet

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdNet done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS)
	@echo Creating archive $(LIBRARY) 
	@rm -f $(LIBRARY)
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	$(CC) -xar -o $(LIBRARY) $(OBJECTS); \
	else \
	ar -rc $(LIBRARY) $(OBJECTS); \
fi

$(OBJDIR)/XrdNet.o: XrdNet.cc       XrdNet.hh         XrdNetBuffer.hh \
                    XrdNetDNS.hh    XrdNetOpts.hh     XrdNetSecurity.hh \
                    XrdNetSocket.hh XrdOucPlatform.hh XrdOucError.hh
	@echo Compiling XrdNet.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdNet.cc -o $(OBJDIR)/XrdNet.o

$(OBJDIR)/XrdNetBuffer.o: XrdNetBuffer.cc XrdNetBuffer.hh \
                          XrdOucChain.hh  XrdOucPlatform.hh  XrdOucPthread.hh
	@echo Compiling XrdNetBuffer.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdNetBuffer.cc -o $(OBJDIR)/XrdNetBuffer.o

$(OBJDIR)/XrdNetConnect.o: XrdNetConnect.cc XrdNetConnect.hh \
                           XrdOucPthread.hh XrdOucTimer.hh
	@echo Compiling XrdNetConnect.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdNetConnect.cc -o $(OBJDIR)/XrdNetConnect.o

$(OBJDIR)/XrdNetDNS.o: XrdNetDNS.cc XrdNetDNS.hh
	@echo Compiling XrdNetDNS.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdNetDNS.cc -o $(OBJDIR)/XrdNetDNS.o

$(OBJDIR)/XrdNetLink.o: XrdNetLink.cc XrdNetLink.hh \
                        XrdNetBuffer.hh  XrdNetDNS.hh       XrdNetPeer.hh \
                        XrdOucChain.hh   XrdOucError.hh     XrdOucPthread.hh \
                        XrdOucStream.hh  XrdOucTokenizer.hh
	@echo Compiling XrdNetLink.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdNetLink.cc -o $(OBJDIR)/XrdNetLink.o

$(OBJDIR)/XrdNetSecurity.o: XrdNetSecurity.cc XrdNetSecurity.hh \
                            XrdOucHash.hh     XrdOucHash.icc \
                            XrdOucNList.hh    XrdOucPthread.hh XrdOucTrace.hh
	@echo Compiling XrdNetSecurity.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdNetSecurity.cc -o $(OBJDIR)/XrdNetSecurity.o

$(OBJDIR)/XrdNetSocket.o: XrdNetSocket.cc  XrdNetSocket.hh \
                          XrdNetConnect.hh XrdNetDNS.hh        XrdNetOpts.hh \
                          XrdOucError.hh   XrdOucPlatform.hh
	@echo Compiling XrdNetSocket.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdNetSocket.cc -o $(OBJDIR)/XrdNetSocket.o

$(OBJDIR)/XrdNetWork.o: XrdNetWork.cc     XrdNetWork.hh     XrdNetBuffer.hh \
                        XrdNetLink.hh     XrdNetOpts.hh     XrdNetPeer.hh   \
                        XrdNet.hh         XrdOucError.hh
	@echo Compiling XrdNetWork.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) XrdNetWork.cc -o $(OBJDIR)/XrdNetWork.o
