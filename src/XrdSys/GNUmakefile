#              $Id$

#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
SOURCES = \
        XrdSysDir.cc \
        XrdSysPlatform.cc \
        XrdSysPlugin.cc   \
        XrdSysPriv.cc     \
        XrdSysProg.cc        \
        XrdSysPthread.cc     \
        XrdSysTimer.cc       \
        XrdSysXSLock.cc

OBJECTS = \
        $(OBJDIR)/XrdSysDir.o \
        $(OBJDIR)/XrdSysPlatform.o \
        $(OBJDIR)/XrdSysPlugin.o       \
        $(OBJDIR)/XrdSysPriv.o    \
        $(OBJDIR)/XrdSysProg.o         \
        $(OBJDIR)/XrdSysPthread.o     \
        $(OBJDIR)/XrdSysTimer.o        \
        $(OBJDIR)/XrdSysXSLock.o

LIBRARY = $(LIBDIR)/libXrdSys.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdOuc% ../XrdOuc

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdSys done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS)
	@echo Creating archive $(LIBRARY) 
	$(ECHO)rm -f $(LIBRARY)
	$(ECHO)if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
	$(CC) -xar -o $(LIBRARY) $(OBJECTS); \
	else \
	ar -rc $(LIBRARY) $(OBJECTS); \
	ranlib $(LIBRARY); \
fi

$(OBJDIR)/XrdSysDir.o: XrdSysDir.cc  XrdSysDir.hh
	@echo Compiling XrdSysDir.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdSysDir.cc -o $(OBJDIR)/XrdSysDir.o

$(OBJDIR)/XrdSysPlatform.o: XrdSysPlatform.cc  XrdSysPlatform.hh
	@echo Compiling XrdSysPlatform.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(CFSETRESUID) $(INCLUDE) XrdSysPlatform.cc -o $(OBJDIR)/XrdSysPlatform.o

$(OBJDIR)/XrdSysPlugin.o: XrdSysPlugin.cc XrdSysPlugin.hh  XrdOucError.hh
	@echo Compiling XrdSysPlugin.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(CFSTRLCPY) $(INCLUDE) XrdSysPlugin.cc -o $(OBJDIR)/XrdSysPlugin.o

$(OBJDIR)/XrdSysPriv.o: XrdSysPriv.cc  XrdSysPriv.hh
	@echo Compiling XrdSysPriv.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(CFSETRESUID) $(INCLUDE) XrdSysPriv.cc -o $(OBJDIR)/XrdSysPriv.o

$(OBJDIR)/XrdSysProg.o: XrdSysProg.cc  XrdSysProg.hh \
                        XrdOucError.hh XrdOucStream.hh XrdSysPthread.hh
	@echo Compiling XrdSysProg.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdSysProg.cc -o $(OBJDIR)/XrdSysProg.o

$(OBJDIR)/XrdSysPthread.o: XrdSysPthread.cc XrdSysPthread.hh XrdOucError.hh
	@echo Compiling XrdSysPthread.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdSysPthread.cc -o $(OBJDIR)/XrdSysPthread.o

$(OBJDIR)/XrdSysTimer.o: XrdSysTimer.cc XrdSysTimer.hh  
	@echo Compiling XrdSysTimer.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdSysTimer.cc -o $(OBJDIR)/XrdSysTimer.o

$(OBJDIR)/XrdSysXSLock.o: XrdOucError.hh XrdSysXSLock.hh XrdSysXSLock.cc
	@echo Compiling XrdSysXSLock.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdSysXSLock.cc -o $(OBJDIR)/XrdSysXSLock.o
