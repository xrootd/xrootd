#              $Id$

#-----------------------------------------------------------------------------#
#                            E n v i r o n m e n t                            #
#-----------------------------------------------------------------------------#
 
include ../GNUmake.env

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdBuffer.cc      \
        XrdConfig.cc      \
        XrdInet.cc        \
        XrdInfo.cc        \
        XrdLink.cc        \
        XrdMain.cc        \
        XrdPoll.cc        \
        XrdProtocol.cc    \
        XrdScheduler.cc   \
        XrdStats.cc

  
OBJECTS = \
        $(OBJDIR)/XrdBuffer.o      \
        $(OBJDIR)/XrdConfig.o      \
        $(OBJDIR)/XrdInet.o        \
        $(OBJDIR)/XrdInfo.o        \
        $(OBJDIR)/XrdLink.o        \
        $(OBJDIR)/XrdMain.o        \
        $(OBJDIR)/XrdPoll.o        \
        $(OBJDIR)/XrdProtocol.o    \
        $(OBJDIR)/XrdScheduler.o   \
        $(OBJDIR)/XrdStats.o

LIBRARY = $(LIBDIR)/libXrd.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                           -
#------------------------------------------------------------------------------#

vpath XrdNet% ../XrdNet
vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make Xrd done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS)
	@echo Creating archive $(LIBRARY) 
	$(ECHO)rm -f $(LIBRARY);
	$(ECHO)if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	$(CC) -xar -o $(LIBRARY) $(OBJECTS); \
else \
	ar -rc $(LIBRARY) $(OBJECTS); \
fi

$(OBJDIR)/XrdBuffer.o:    XrdBuffer.cc   XrdBuffer.hh     XrdTrace.hh \
                          XrdOucError.hh XrdOucPthread.hh XrdOucTrace.hh \
                          XrdOucTimer.hh
	@echo Compiling XrdBuffer.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdBuffer.o XrdBuffer.cc

$(OBJDIR)/XrdConfig.o: XrdBuffer.hh      XrdConfig.hh      XrdJob.hh     \
                       XrdLink.hh        XrdInet.hh        XrdObject.hh  \
                       XrdPoll.hh        XrdProtocol.hh    XrdScheduler.hh \
                       XrdStats.hh       XrdTrace.hh       XrdInfo.hh    \
                       XrdOuca2x.hh      XrdOucError.hh    XrdOucLogger.hh   \
                       XrdOucStream.hh   XrdOucTrace.hh    XrdOucTimer.hh \
                       XrdNetSecurity.hh XrdNetDNS.hh      XrdConfig.cc \
                       ../XrdVersion.hh
	@echo Compiling XrdConfig.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdConfig.o XrdConfig.cc

$(OBJDIR)/XrdInet.o:    XrdInet.hh         XrdInet.cc   \
                        XrdLink.hh         XrdTrace.hh  \
                        XrdOucError.hh     XrdOucLogger.hh  XrdOucTrace.hh \
                        XrdNet.hh          XrdNetOpts.hh    XrdNetPeer.hh
	@echo Compiling XrdInet.cc
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdInet.o XrdInet.cc

$(OBJDIR)/XrdInfo.o: XrdInfo.hh
	@echo Compiling XrdInfo.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdInfo.o XrdInfo.cc

$(OBJDIR)/XrdLink.o:   XrdBuffer.hh     XrdJob.hh          XrdLink.hh      \
                       XrdObject.hh     XrdObject.icc  \
                       XrdProtocol.hh   XrdScheduler.hh    XrdTrace.hh  \
                       XrdOucError.hh   XrdOucPlatform.hh  XrdOucPthread.hh \
                       XrdOucTrace.hh   XrdOucTimer.hh     XrdLink.cc \
                       XrdNetDNS.hh     XrdNetPeer.hh
	@echo Compiling XrdLink.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdLink.o XrdLink.cc

$(OBJDIR)/XrdMain.o:   XrdConfig.hh    XrdLink.hh        XrdInet.hh \
                       XrdPoll.hh      XrdScheduler.hh   XrdTrace.hh    \
                       XrdOucError.hh  XrdOucLogger.hh   XrdOucPthread.hh \
                       XrdOucTrace.hh    \
                       XrdMain.cc
	@echo Compiling XrdMain.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdMain.o XrdMain.cc

$(OBJDIR)/XrdPoll.o:   XrdLink.hh       XrdPoll.hh       XrdProtocol.hh   \
                       XrdPoll.cc       XrdPollDev.hh    XrdPollDev.icc \
                       XrdPollPoll.hh   XrdPollDev.icc   XrdTrace.hh     \
                       XrdOucError.hh   XrdOucPthread.hh XrdOucTrace.hh
	@echo Compiling XrdPoll.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(CFDEVPOLL) $(INCLUDE) -o $(OBJDIR)/XrdPoll.o XrdPoll.cc

$(OBJDIR)/XrdProtocol.o: XrdJob.hh        XrdLink.hh     XrdPoll.hh \
                         XrdProtocol.cc   XrdProtocol.hh XrdTrace.hh \
                         XrdOucError.hh   XrdOucTrace.hh
	@echo Compiling XrdProtocol.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdProtocol.o XrdProtocol.cc

$(OBJDIR)/XrdScheduler.o: XrdJob.hh      XrdScheduler.hh   XrdTrace.hh \
                          XrdOucError.hh XrdOucPthread.hh  XrdOucTrace.hh \
                          XrdScheduler.cc
	@echo Compiling XrdScheduler.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdScheduler.o XrdScheduler.cc

$(OBJDIR)/XrdStats.o: XrdBuffer.hh     XrdLink.hh     XrdPoll.hh \
                      XrdProtocol.hh   XrdStats.cc    XrdStats.hh \
                      XrdOucPthread.hh ../XrdVersion.hh
	@echo Compiling XrdStats.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdStats.o XrdStats.cc
