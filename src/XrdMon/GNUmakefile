#              $Id$

#-----------------------------------------------------------------------------#
#                       C o m m o n   V a r i a b l e s                       #
#-----------------------------------------------------------------------------#
  
include ../GNUmake.env

#-----------------------------------------------------------------------------#
#                             C o m p o n e n t s                             #
#-----------------------------------------------------------------------------#
  
OBJECTS_LIB_COMMON =                          \
        $(OBJDIR)/XrdMonException.o           \
        $(OBJDIR)/XrdMonHeader.o              \
        $(OBJDIR)/XrdMonTimer.o               \
        $(OBJDIR)/XrdMonUtils.o

OBJECTS_LIB_CTR =                             \
	$(OBJDIR)/XrdMonCtrAdmin.o            \
	$(OBJDIR)/XrdMonCtrArchiver.o         \
	$(OBJDIR)/XrdMonCtrBuffer.o           \
	$(OBJDIR)/XrdMonCtrDebug.o            \
	$(OBJDIR)/XrdMonCtrCollector.o        \
	$(OBJDIR)/XrdMonCtrSenderInfo.o       \
	$(OBJDIR)/XrdMonCtrWriter.o

OBJECTS_LIB_DEC =                             \
	$(OBJDIR)/XrdMonDecArgParser.o        \
        $(OBJDIR)/XrdMonDecDictInfo.o         \
        $(OBJDIR)/XrdMonDecPacketDecoder.o    \
        $(OBJDIR)/XrdMonDecSink.o             \
	$(OBJDIR)/XrdMonDecTraceInfo.o


OBJECTS_LIB_SND =                             \
	$(OBJDIR)/XrdMonSndCoder.o            \
	$(OBJDIR)/XrdMonSndDebug.o            \
	$(OBJDIR)/XrdMonSndDictEntry.o        \
	$(OBJDIR)/XrdMonSndDummyXrootd.o      \
	$(OBJDIR)/XrdMonSndPacket.o           \
	$(OBJDIR)/XrdMonSndTraceCache.o       \
	$(OBJDIR)/XrdMonSndTraceEntry.o       \
	$(OBJDIR)/XrdMonSndTransmitter.o

OBJECTS_BIN_CTR = $(OBJDIR)/XrdMonCtrMainApp.o
OBJECTS_BIN_ADM = $(OBJDIR)/XrdMonSndAdminApp.o
OBJECTS_BIN_SND = $(OBJDIR)/XrdMonSndDummyXrootdApp.o
OBJECTS_BIN_DMP = $(OBJDIR)/XrdMonDumpPacketsApp.o

LIBRARY_OUC    = $(LIBDIR)/libXrdOuc.a

LIBRARY_COMMON = $(LIBDIR)/libXrdMonCommon.a
LIBRARY_DEC    = $(LIBDIR)/libXrdMonDecoder.a
LIBRARY_CTR    = $(LIBDIR)/libXrdMonCollector.a
LIBRARY_SND    = $(LIBDIR)/libXrdMonDummySender.a

LIBRARIES      = $(LIBRARY_COMMON) $(LIBRARY_CTR) \
                 $(LIBRARY_DEC) $(LIBRARY_SND) 

COLLECTOR      = $(BINDIR)/xrdmoncollector
SENDER         = $(BINDIR)/xrdmondummysender
ADMIN          = $(BINDIR)/xrdmonadmin
DUMPPACKETS    = $(BINDIR)/xrdmondumppackets
BINARIES       = $(COLLECTOR) $(SENDER) $(ADMIN) \
                 $(DUMPPACKETS)

TARGETS        = $(LIBRARIES) $(BINARIES)

#-----------------------------------------------------------------------------#
#                           S e a r c h   P a t h s                           #
#-----------------------------------------------------------------------------#

vpath XrdMon% ../XrdMon

#-----------------------------------------------------------------------------#
#                          I n i t i a l   R u l e s                          #
#-----------------------------------------------------------------------------#
 
include ../GNUmake.options

anything: $(TARGETS)
	@echo Make XrdMon done.

#-----------------------------------------------------------------------------#
#                           D e p e n d e n c i e s                           #
#-----------------------------------------------------------------------------#

$(LIBRARY_COMMON): $(OBJECTS_LIB_COMMON)
	@echo Creating archive $(LIBRARY_COMMON) 
	@rm -f $(LIBRARY_COMMON)
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
		$(CC) -xar -o $(LIBRARY_COMMON) $(OBJECTS_LIB_COMMON); \
	else \
		ar -rc $(LIBRARY_COMMON) $(OBJECTS_LIB_COMMON); \
		ranlib $(LIBRARY_COMMON); \
	fi

$(LIBRARY_CTR):	$(OBJECTS_LIB_CTR)
	@echo Creating archive $(LIBRARY_CTR) 
	@rm -f $(LIBRARY_CTR)
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
		$(CC) -xar -o $(LIBRARY_CTR) $(OBJECTS_LIB_CTR); \
	else \
		ar -rc $(LIBRARY_CTR) $(OBJECTS_LIB_CTR); \
		ranlib $(LIBRARY_CTR); \
	fi

$(LIBRARY_DEC):	$(OBJECTS_LIB_DEC)
	@echo Creating archive $(LIBRARY_DEC) 
	@rm -f $(LIBRARY_DEC)
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
		$(CC) -xar -o $(LIBRARY_DEC) $(OBJECTS_LIB_DEC); \
	else \
		ar -rc $(LIBRARY_DEC) $(OBJECTS_LIB_DEC); \
		ranlib $(LIBRARY_DEC); \
	fi

$(LIBRARY_SND): $(OBJECTS_LIB_SND)
	@echo Creating archive $(LIBRARY_SND) 
	@rm -f $(LIBRARY_SND)
	@if [ "$(TYPE)" = "SunOS" -a "$(CC)" = "CC" ]; then \
		$(CC) -xar -o $(LIBRARY_SND) $(OBJECTS_LIB_SND); \
	else \
		ar -rc $(LIBRARY_SND) $(OBJECTS_LIB_SND); \
		ranlib $(LIBRARY_SND); \
	fi

$(COLLECTOR):	$(OBJECTS_BIN_CTR) $(LIBRARY_CTR) $(LIBRARY_OUC) \
	$(LIBRARY_DEC) $(LIBRARY_COMMON) 
	@echo Creating executable $(@F)
	$(ECHO)$(CC) $(CFLAGS) $(LDOP) $(LIBS) -o $@ $^

$(SENDER):	$(OBJECTS_BIN_SND) $(LIBRARY_SND) $(LIBRARY_COMMON) 
	@echo Creating executable $(@F)
	$(ECHO)$(CC) $(CFLAGS) $(LDOP) $(LIBS) -o $@ $^

$(ADMIN):	$(OBJECTS_BIN_ADM) $(LIBRARY_SND) $(LIBRARY_COMMON) 
	@echo Creating executable $(@F)
	$(ECHO)$(CC) $(CFLAGS) $(LDOP) $(LIBS) -o $@ $^

$(DUMPPACKETS):	$(OBJECTS_BIN_DMP) $(LIBRARY_DEC) $(LIBRARY_COMMON) 
	@echo Creating executable $(@F)
	$(ECHO)$(CC) $(CFLAGS) $(LDOP) $(LIBS) -o $@ $^

$(OBJDIR)/%.o : %.cc
	@echo Compiling $(<F)
	$(ECHO)$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $(OBJDIR)/$@

