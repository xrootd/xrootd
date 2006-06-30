#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdOssAio.cc \
        XrdOssApi.cc \
        XrdOssCache.cc \
        XrdOssConfig.cc \
        XrdOssCreate.cc \
        XrdOssLock.cc   \
        XrdOssMio.cc    \
        XrdOssMSS.cc    \
        XrdOssRename.cc \
        XrdOssStage.cc  \
        XrdOssUnlink.cc \
	XrdOssProxy.cc 

OBJECTS = \
        $(OBJDIR)/XrdOssAio.o \
        $(OBJDIR)/XrdOssApi.o \
        $(OBJDIR)/XrdOssCache.o \
        $(OBJDIR)/XrdOssConfig.o \
        $(OBJDIR)/XrdOssCreate.o \
        $(OBJDIR)/XrdOssLock.o   \
        $(OBJDIR)/XrdOssMio.o    \
        $(OBJDIR)/XrdOssMSS.o    \
        $(OBJDIR)/XrdOssRename.o \
        $(OBJDIR)/XrdOssStage.o  \
        $(OBJDIR)/XrdOssUnlink.o \
	$(OBJDIR)/XrdOssProxy.o	

LIBRARY = $(LIBDIR)/libXrdOss.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdNet% ../XrdNet
vpath XrdOfs% ../XrdOfs
vpath XrdOuc% ../XrdOuc
vpath XrdSfs% ../XrdSfs

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdOss done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS)
	@echo Creating archive $(LIBRARY)
	$(ECHO)rm -f $(LIBRARY)
	$(ECHO)ar -rc $(LIBRARY) $(OBJECTS)
	$(ECHO)ranlib $(LIBRARY)
	$(ECHO)if [ "x$(SUNCACHE)" != "x" ]; then \
	   if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	      ar -rc $(LIBRARY) $(OBJDIR)$(SUNCACHE)/*/*.o; \
	   fi;\
	fi

$(OBJDIR)/XrdOssAio.o: XrdOssAio.cc    XrdOssTrace.hh  XrdOssApi.hh \
                       XrdOucError.hh  XrdOucPthread.hh XrdOucPlatform.hh \
                       XrdSfsAio.hh
	@echo Compiling XrdOssAio.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(CFSIGWTI) $(INCLUDE) -o $(OBJDIR)/XrdOssAio.o XrdOssAio.cc

$(OBJDIR)/XrdOssApi.o: XrdOss.hh XrdOssApi.hh XrdOssCache.hh XrdOssConfig.hh \
                       XrdOssError.hh  XrdOssLock.hh     XrdOssTrace.hh \
                       XrdOssMio.hh    XrdOssMioFile.hh \
                       XrdOucError.hh  XrdOucPList.hh    XrdOucPthread.hh \
                       XrdOucStream.hh XrdOucPlatform.hh XrdOssApi.cc \
                       XrdOucName2Name.hh XrdOucPlugin.hh \
                       ../XrdVersion.hh
	@echo Compiling XrdOssApi.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssApi.o XrdOssApi.cc

$(OBJDIR)/XrdOssCache.o: XrdOssApi.hh XrdOssCache.hh XrdOssTrace.hh \
                         XrdOucDLlist.hh XrdOucError.hh \
                         XrdOssCache.cc
	@echo Compiling XrdOssCache.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssCache.o XrdOssCache.cc

$(OBJDIR)/XrdOssConfig.o: XrdOssApi.hh  XrdOssConfig.hh XrdOssError.hh \
                          XrdOssLock.hh XrdOssTrace.hh XrdOucError.hh \
                          XrdOssMio.hh \
                          XrdOucProg.hh XrdOssConfig.cc XrdOuca2x.hh  \
                          XrdOucName2Name.hh XrdOucPlugin.hh
	@echo Compiling XrdOssConfig.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssConfig.o XrdOssConfig.cc

$(OBJDIR)/XrdOssCreate.o: XrdOssApi.hh XrdOssConfig.hh  XrdOssError.hh \
                          XrdOssLock.hh XrdOssTrace.hh  XrdOssOpaque.hh \
                          XrdOucEnv.hh  XrdOucError.hh  XrdOuca2x.hh  \
                          XrdOucPlatform.hh XrdOssCreate.cc
	@echo Compiling XrdOssCreate.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssCreate.o XrdOssCreate.cc

$(OBJDIR)/XrdOssLock.o:   XrdOssApi.hh XrdOssConfig.hh XrdOssError.hh \
                          XrdOssLock.hh XrdOucError.hh XrdOucPlatform.hh \
                          XrdOssLock.cc
	@echo Compiling XrdOssLock.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssLock.o XrdOssLock.cc

$(OBJDIR)/XrdOssMio.o:    XrdOssMio.cc   XrdOssMio.hh    XrdOssMioFile.hh \
                          XrdOssTrace.hh XrdOucError.hh  XrdOucPthread.hh \
                          XrdOucHash.hh  XrdOucHash.icc
	@echo Compiling XrdOssMio.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssMio.o XrdOssMio.cc

$(OBJDIR)/XrdOssMSS.o:    XrdOssApi.hh   XrdOssConfig.hh XrdOssError.hh \
                          XrdOssTrace.hh XrdOucError.hh  XrdOucProg.hh \
                          XrdOucStream.hh XrdOucPlatform.hh  XrdOucLogger.hh \
                          XrdNetOpts.hh   XrdNetSocket.hh \
                          XrdOssMSS.cc
	@echo Compiling XrdOssMSS.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssMSS.o XrdOssMSS.cc

$(OBJDIR)/XrdOssRename.o: XrdOssApi.hh XrdOssError.hh XrdOssLock.hh \
                          XrdOssTrace.hh \
                          XrdOssRename.cc
	@echo Compiling XrdOssRename.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssRename.o XrdOssRename.cc

$(OBJDIR)/XrdOssStage.o: XrdOssApi.hh   XrdOssCache.hh  XrdOssError.hh \
                         XrdOssLock.hh  XrdOssOpaque.hh XrdOucEnv.hh \
                         XrdOuca2x.hh   XrdOucHash.hh   XrdOucHash.icc \
                         XrdOucPthread.hh XrdOucName2Name.hh \
                         XrdOucProg.hh  XrdOucReqID.hh  XrdOssStage.cc
	@echo Compiling XrdOssStage.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssStage.o XrdOssStage.cc

$(OBJDIR)/XrdOssUnlink.o: XrdOssApi.hh XrdOssError.hh XrdOssLock.hh \
                          XrdOssTrace.hh \
                          XrdOssUnlink.cc
	@echo Compiling XrdOssUnlink.cc 
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssUnlink.o XrdOssUnlink.cc

$(OBJDIR)/XrdOssProxy.o: XrdOssConfig.hh XrdOssError.hh \
			 XrdOssLock.hh XrdOucError.hh XrdOucPlatform.hh \
			 XrdOssProxy.cc
	@echo Compiling XrdOssProxy.cc ...
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssProxy.o XrdOssProxy.cc
