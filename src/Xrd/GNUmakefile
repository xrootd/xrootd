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
        XrdLink.cc        \
        XrdMain.cc        \
        XrdNetwork.cc     \
        XrdPoll.cc        \
        XrdProtocol.cc    \
        XrdScheduler.cc   \
        XrdStats.cc       \
        XrdInfo.cc

  
OBJECTS = \
        $(OBJDIR)/XrdBuffer.o      \
        $(OBJDIR)/XrdConfig.o      \
        $(OBJDIR)/XrdLink.o        \
        $(OBJDIR)/XrdMain.o        \
        $(OBJDIR)/XrdNetwork.o     \
        $(OBJDIR)/XrdPoll.o        \
        $(OBJDIR)/XrdProtocol.o    \
        $(OBJDIR)/XrdScheduler.o   \
        $(OBJDIR)/XrdStats.o       \
        $(OBJDIR)/XrdInfo.o

LIBRARY = $(LIBDIR)/libXrd.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                           -
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.rules

anything: $(TARGETS)
	@echo Make Xrd done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS)
	@echo Creating archive $(LIBRARY) 
	@rm -f $(LIBRARY);
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	$(CC) -xar -o $(LIBRARY) $(OBJECTS); \
else \
	ar -rc $(LIBRARY) $(OBJECTS); \
fi

$(OBJDIR)/XrdBuffer.o:    XrdBuffer.cc   XrdBuffer.hh     XrdTrace.hh \
                          XrdOucError.hh XrdOucPthread.hh XrdOucTrace.hh \
                          XrdOucTimer.hh
	@echo Compiling XrdBuffer.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdBuffer.o XrdBuffer.cc

$(OBJDIR)/XrdConfig.o: XrdBuffer.hh      XrdConfig.hh      XrdJob.hh     \
                       XrdLink.hh        XrdNetwork.hh     XrdObject.hh  \
                       XrdPoll.hh        XrdProtocol.hh    XrdScheduler.hh \
                       XrdStats.hh       XrdTrace.hh       XrdInfo.hh    \
                       XrdOuca2x.hh      XrdOucError.hh    XrdOucLogger.hh   \
                       XrdOucSecurity.hh XrdOucStream.hh   XrdOucTrace.hh \
                       XrdOucTimer.hh    ../XrdVersion.hh  XrdConfig.cc
	@echo Compiling XrdConfig.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdConfig.o XrdConfig.cc

$(OBJDIR)/XrdLink.o:   XrdBuffer.hh     XrdJob.hh          XrdLink.hh      \
                       XrdNetwork.hh    XrdObject.hh       XrdObject.icc  \
                       XrdProtocol.hh   XrdScheduler.hh    XrdTrace.hh  \
                       XrdOucError.hh   XrdOucPlatform.hh  XrdOucPthread.hh \
                       XrdOucTrace.hh   XrdOucTimer.hh     XrdLink.cc
	@echo Compiling XrdLink.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdLink.o XrdLink.cc

$(OBJDIR)/XrdMain.o:   XrdConfig.hh    XrdLink.hh        XrdNetwork.hh  \
                       XrdPoll.hh      XrdScheduler.hh   XrdTrace.hh    \
                       XrdOucError.hh  XrdOucLogger.hh   XrdOucPthread.hh \
                       XrdOucTrace.hh    \
                       XrdMain.cc
	@echo Compiling XrdMain.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdMain.o XrdMain.cc

$(OBJDIR)/XrdNetwork.o: XrdBuffer.hh       XrdLink.hh       XrdNetwork.hh   \
                        XrdTrace.hh  \
                        XrdOucError.hh     XrdOucLogger.hh  XrdOucPlatform.hh \
                        XrdOucPthread.hh \
                        XrdOucSecurity.hh  XrdOucTrace.hh   XrdNetwork.cc
	@echo Compiling XrdNetwork.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdNetwork.o XrdNetwork.cc

$(OBJDIR)/XrdPoll.o:   XrdLink.hh       XrdPoll.hh       XrdProtocol.hh   \
                       XrdPoll.cc       XrdPollDev.hh    XrdPollDev.icc \
                       XrdPollPoll.hh   XrdPollDev.icc   XrdTrace.hh     \
                       XrdOucError.hh   XrdOucPthread.hh XrdOucTrace.hh
	@echo Compiling XrdPoll.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdPoll.o XrdPoll.cc

$(OBJDIR)/XrdProtocol.o: XrdJob.hh        XrdLink.hh     XrdPoll.hh \
                         XrdProtocol.cc   XrdProtocol.hh XrdTrace.hh \
                         XrdOucError.hh   XrdOucTrace.hh
	@echo Compiling XrdProtocol.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdProtocol.o XrdProtocol.cc

$(OBJDIR)/XrdScheduler.o: XrdJob.hh      XrdScheduler.hh   XrdTrace.hh \
                          XrdOucError.hh XrdOucPthread.hh  XrdOucTrace.hh \
                          XrdScheduler.cc
	@echo Compiling XrdScheduler.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdScheduler.o XrdScheduler.cc

$(OBJDIR)/XrdStats.o: XrdBuffer.hh     XrdLink.hh     XrdPoll.hh \
                      XrdProtocol.hh   XrdStats.cc    XrdStats.hh \
                      XrdOucPthread.hh ../XrdVersion.hh
	@echo Compiling XrdStats.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdStats.o XrdStats.cc

$(OBJDIR)/XrdInfo.o: XrdInfo.hh
	@echo Compiling XrdInfo.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdInfo.o XrdInfo.cc
