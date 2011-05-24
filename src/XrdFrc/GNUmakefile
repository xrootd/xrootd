#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdFrcCID.cc       \
        XrdFrcProxy.cc     \
        XrdFrcReqAgent.cc  \
        XrdFrcReqFile.cc   \
        XrdFrcTrace.cc     \
        XrdFrcUtils.cc

OBJECTS = \
        $(OBJDIR)/XrdFrcCID.o       \
        $(OBJDIR)/XrdFrcProxy.o     \
        $(OBJDIR)/XrdFrcReqAgent.o  \
        $(OBJDIR)/XrdFrcReqFile.o   \
        $(OBJDIR)/XrdFrcTrace.o     \
        $(OBJDIR)/XrdFrcUtils.o

LIBRARY = $(LIBDIR)/libXrdFrc.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdCms% ../XrdCms
vpath XrdFrc% ../XrdFrc
vpath XrdNet% ../XrdNet
vpath XrdOss% ../XrdOss
vpath XrdOuc% ../XrdOuc
vpath XrdSys% ../XrdSys
vpath XrdXrootd% ../XrdXrootd
vpath Xrd%    ../Xrd
vpath XP%     ../XProtocol

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdFrc done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(LIBRARY): $(OBJECTS)
	@echo Creating archive $(LIBRARY)
	$(ECHO)rm -f $(LIBRARY)
	$(ECHO)ar -rc $(LIBRARY) $(OBJECTS)
	$(ECHO)ranlib $(LIBRARY)

$(OBJDIR)/XrdFrcCID.o: XrdFrcCID.hh    XrdFrcCID.cc    XrdFrcTrace.hh \
                       XrdOucEnv.hh    XrdOucStream.hh \
                       XrdSysError.hh  XrdSysPlatform.hh
	@echo Compiling XrdFrcCID.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdFrcCID.o XrdFrcCID.cc

$(OBJDIR)/XrdFrcProxy.o: XrdFrcProxy.hh       XrdFrcProxy.cc \
                         XrdFrcReqAgent.hh    XrdFrcRequest.hh \
                         XrdFrcTrace.hh       XrdFrcUtils.hh \
                         XrdOucEnv.hh         XrdOucStream.hh \
                         XrdOucUtils.hh \
                         XrdSysError.hh       XrdSysLogger.hh \
                         XrdSysPlatform.hh
	@echo Compiling XrdFrcProxy.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdFrcProxy.o XrdFrcProxy.cc

$(OBJDIR)/XrdFrcReqAgent.o: XrdFrcReqAgent.hh XrdFrcReqAgent.cc \
                            XrdFrcReqFile.hh  XrdFrcRequest.hh \
                            XrdFrcTrace.hh    XrdFrcUtils.hh \
                            XrdNetMsg.hh  \
                            XrdSysError.hh    XrdSysHeaders.hh \
                            XrdSysPlatform.hh
	@echo Compiling XrdFrcReqAgent.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdFrcReqAgent.o XrdFrcReqAgent.cc

$(OBJDIR)/XrdFrcReqFile.o: XrdFrcReqFile.cc      XrdFrcReqFile.hh \
                           XrdFrcRequest.hh      XrdFrcCID.hh \
                           XrdFrcTrace.hh \
                           XrdSysError.hh        XrdSysPthread.hh \
                           XrdSysPlatform.hh
	@echo Compiling XrdFrcReqFile.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdFrcReqFile.o XrdFrcReqFile.cc


$(OBJDIR)/XrdFrcTrace.o:  XrdFrcTrace.hh    XrdFrcTrace.cc
	@echo Compiling XrdFrcTrace.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdFrcTrace.o XrdFrcTrace.cc

$(OBJDIR)/XrdFrcUtils.o:  XrdFrcUtils.hh    XrdFrcUtils.cc \
                          XrdFrcRequest.hh  XrdFrcTrace.hh   XrdFrcXAttr.hh \
                          XrdOucSxeq.hh     XrdOucUtils.hh   XrdOucXAttr.hh \
                          XrdSysError.hh    XrdSysHeaders.hh XrdSysPlatform.hh
	@echo Compiling XrdFrcUtils.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdFrcUtils.o XrdFrcUtils.cc
