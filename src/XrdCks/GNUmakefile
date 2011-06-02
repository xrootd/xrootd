#------------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                        #
#------------------------------------------------------------------------------#
  
include ../GNUmake.env

#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#
  
SOURCES = \
        XrdCksCalccrc32.cc \
        XrdCksCalcmd5.cc   \
        XrdCksConfig.cc    \
        XrdCksManager.cc

OBJECTS = \
        $(OBJDIR)/XrdCkscrc32.o \
        $(OBJDIR)/XrdCksmd5.o   \
        $(OBJDIR)/XrdCksConfig.o\
        $(OBJDIR)/XrdCksManager.o

LIBRARY = $(LIBDIR)/libXrdCks.a

TARGETS = $(LIBRARY)

#------------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                            #
#------------------------------------------------------------------------------#

vpath XrdCks% ../XrdCks
vpath XrdNet% ../XrdNet
vpath XrdOuc% ../XrdOuc
vpath XrdSys% ../XrdSys
vpath Xrd% ../Xrd

#------------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                           #
#------------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdCks done.

#------------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                            #
#------------------------------------------------------------------------------#

$(TARGETS): $(OBJECTS)
	@echo Creating archive $(LIBRARY) 
	$(ECHO)rm -f $(LIBRARY)
	$(ECHO)ar -rc $(LIBRARY) $(OBJECTS)
	$(ECHO)ranlib $(LIBRARY)

$(OBJDIR)/XrdCkscrc32.o: XrdCksCalccrc32.cc XrdCksCalccrc32.hh \
                         XrdCksCalc.hh      XrdSysPlatform.hh
	@echo Compiling XrdCksCalccrc32.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdCksCalccrc32.cc -o $(OBJDIR)/XrdCkscrc32.o

$(OBJDIR)/XrdCksmd5.o: XrdCksCalcmd5.cc XrdCksCalcmd5.hh \
                       XrdCksCalc.hh    XrdSysPlatform.hh
	@echo Compiling XrdCksCalcmd5.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdCksCalcmd5.cc -o $(OBJDIR)/XrdCksmd5.o

$(OBJDIR)/XrdCksConfig.o: XrdCksConfig.cc XrdCksConfig.hh   XrdCks.hh \
                          XrdCksData.hh   XrdCksManager.hh \
                          XrdOucStream.hh XrdOucTList.hh \
                          XrdSysError.hh  XrdSysPlugin.hh
	@echo Compiling XrdCksConfig.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) XrdCksConfig.cc -o $(OBJDIR)/XrdCksConfig.o

$(OBJDIR)/XrdCksManager.o: XrdCksManager.cc    XrdCksManager.hh      XrdCks.hh \
                           XrdCksCalc.hh       XrdCksCalcadler32.hh \
                           XrdCksCalccrc32.hh  XrdCksCalcmd5.hh \
                           XrdCksData.hh       XrdCksXAttr.hh \
                           XrdOucTokenizer.hh  XrdOucXAttr.hh \
                           XrdSysError.hh      XrdSysFAttr.hh \
                           XrdSysPlugin.hh
	@echo Compiling XrdCksManager.cc
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) -o $(OBJDIR)/XrdCksManager.o XrdCksManager.cc
