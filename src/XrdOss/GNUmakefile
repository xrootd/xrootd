#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdOssApi.cc \
        XrdOssCache.cc \
        XrdOssConfig.cc \
        XrdOssCreate.cc \
        XrdOssLock.cc   \
        XrdOssMSS.cc    \
        XrdOssRename.cc \
        XrdOssStage.cc  \
        XrdOssUnlink.cc \
	XrdOssProxy.cc 

OBJECTS = \
        $(OBJDIR)/XrdOssApi.o \
        $(OBJDIR)/XrdOssCache.o \
        $(OBJDIR)/XrdOssConfig.o \
        $(OBJDIR)/XrdOssCreate.o \
        $(OBJDIR)/XrdOssLock.o   \
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

vpath XrdOfs% ../XrdOfs
vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.rules

anything: $(TARGETS)
	@echo Make XrdOss done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS)
	@echo Creating archive $(LIBRARY)
	@rm -f $(LIBRARY)
	@ar -rc $(LIBRARY) $(OBJECTS)
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	 ar -rc $(LIBRARY) $(OBJDIR)/SunWS_cache/*/*.o; \
fi

$(OBJDIR)/XrdOssApi.o: XrdOss.hh XrdOssApi.hh XrdOssCache.hh XrdOssConfig.hh \
                       XrdOssError.hh XrdOssLock.hh XrdOssTrace.hh \
                       XrdOucError.hh XrdOucPList.hh XrdOucStream.hh \
                       XrdOucPlatform.hh XrdOssApi.cc
	@echo Compiling XrdOssApi.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssApi.o XrdOssApi.cc

$(OBJDIR)/XrdOssCache.o: XrdOssApi.hh XrdOssCache.hh XrdOssTrace.hh \
                         XrdOucDLlist.hh XrdOucError.hh \
                         XrdOssCache.cc
	@echo Compiling XrdOssCache.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssCache.o XrdOssCache.cc

$(OBJDIR)/XrdOssConfig.o: XrdOssApi.hh XrdOssConfig.hh XrdOssError.hh \
                          XrdOssLock.hh XrdOssTrace.hh XrdOucError.hh \
                          XrdOssConfig.cc XrdOuca2x.hh
	@echo Compiling XrdOssConfig.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssConfig.o XrdOssConfig.cc

$(OBJDIR)/XrdOssCreate.o: XrdOssApi.hh XrdOssConfig.hh  XrdOssError.hh \
                          XrdOssLock.hh XrdOssTrace.hh  XrdOssOpaque.hh \
                          XrdOucEnv.hh  XrdOucError.hh  XrdOuca2x.hh  \
                          XrdOssCreate.cc
	@echo Compiling XrdOssCreate.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssCreate.o XrdOssCreate.cc

$(OBJDIR)/XrdOssLock.o:   XrdOssApi.hh XrdOssConfig.hh XrdOssError.hh \
                          XrdOssLock.hh XrdOucError.hh XrdOucPlatform.hh \
                          XrdOssLock.cc
	@echo Compiling XrdOssLock.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssLock.o XrdOssLock.cc

$(OBJDIR)/XrdOssMSS.o:    XrdOssApi.hh XrdOssConfig.hh XrdOssError.hh \
                          XrdOssTrace.hh XrdOucError.hh XrdOucSocket.hh \
                          XrdOucStream.hh XrdOucPlatform.hh  XrdOucLogger.hh \
                          XrdOssMSS.cc
	@echo Compiling XrdOssMSS.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssMSS.o XrdOssMSS.cc

$(OBJDIR)/XrdOssRename.o: XrdOssApi.hh XrdOssError.hh XrdOssLock.hh \
                          XrdOssTrace.hh \
                          XrdOssRename.cc
	@echo Compiling XrdOssRename.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssRename.o XrdOssRename.cc

$(OBJDIR)/XrdOssStage.o: XrdOssApi.hh XrdOssCache.hh XrdOssError.hh \
                         XrdOssLock.hh  XrdOssOpaque.hh XrdOucEnv.hh \
                         XrdOuca2x.hh XrdOssStage.cc
	@echo Compiling XrdOssStage.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssStage.o XrdOssStage.cc

$(OBJDIR)/XrdOssUnlink.o: XrdOssApi.hh XrdOssError.hh XrdOssLock.hh \
                          XrdOssTrace.hh \
                          XrdOssUnlink.cc
	@echo Compiling XrdOssUnlink.cc 
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssUnlink.o XrdOssUnlink.cc

$(OBJDIR)/XrdOssProxy.o: XrdOssConfig.hh XrdOssError.hh \
			 XrdOssLock.hh XrdOucError.hh XrdOucPlatform.hh \
			 XrdOssProxy.cc
	@echo Compiling XrdOssProxy.cc ...
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdOssProxy.o XrdOssProxy.cc