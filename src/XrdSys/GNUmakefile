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
        XrdSysPriv.cc

OBJECTS = \
        $(OBJDIR)/XrdSysDir.o \
        $(OBJDIR)/XrdSysPlatform.o \
        $(OBJDIR)/XrdSysPriv.o

LIBRARY = $(LIBDIR)/libXrdSys.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

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

$(OBJDIR)/XrdSysPriv.o: XrdSysPriv.cc  XrdSysPriv.hh
	@echo Compiling XrdSysPriv.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(CFSETRESUID) $(INCLUDE) XrdSysPriv.cc -o $(OBJDIR)/XrdSysPriv.o
