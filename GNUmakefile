#------------------------------------------------------------------------------#
#                             C o m p o n e n t s                              #
#------------------------------------------------------------------------------#

# Order in which components must be made
#
SERVER  = XrdOuc   XrdSfs   XrdAcc XrdSec XrdSeckrb4 XrdSeckrb5 XrdOdc \
          XrdXr    XrdOss   XrdOfs XrdOlb Xrd        XrdRootd   XrdXrootd

TARGETS = $(SERVER)

ARCH    = `../../GNUmakearch`

TYPE    = `/bin/uname`

#------------------------------------------------------------------------------#
#                                  R u l e s                                   #
#------------------------------------------------------------------------------#

all:
	@$(MAKE) $(TARGETS) MAKEXEQ=all XMSG=Making --no-print-directory
	@/bin/rm -f bin/arch bin/arch_dbg lib/arch lib/arch_dbg
	@/bin/ln -s ./`GNUmakearch`      bin/arch
	@/bin/ln -s ./`GNUmakearch`_dbg bin/arch_dbg
	@/bin/ln -s ./`GNUmakearch`     lib/arch
	@/bin/ln -s ./`GNUmakearch`_dbg lib/arch_dbg
	@echo Make all done

debug: FORCE
	@$(MAKE) $(TARGETS) MAKEXEQ=debug XMSG="Making debug" --no-print-directory
	@echo Make debug done

clean: FORCE
	@$(MAKE) $(TARGETS) MAKEXEQ=clean XMSG=Cleaning --no-print-directory
	@if [ "$(TYPE)" = "SunOS" ]; then \
	/bin/rm -fr obj/`GNUmakearch`/SunWS_cache;\
	/bin/rm -fr obj/`GNUmakearch`/dbg/SunWS_cache;\
fi

XrdAcc: FORCE
	@echo $(XMSG) acc component...
	@cd src/XrdAcc;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdOdc: FORCE
	@echo $(XMSG) odc component...
	@cd src/XrdOdc;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdOfs: FORCE
	@echo $(XMSG) ofs component...
	@cd src/XrdOfs;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdOlb: FORCE
	@echo $(XMSG) olb component...
	@cd src/XrdOlb;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdOss: FORCE
	@echo $(XMSG) oss component...
	@cd src/XrdOss;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdOuc: FORCE
	@echo $(XMSG) ouc component...
	@cd src/XrdOuc;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdSfs: FORCE
	@echo $(XMSG) sfs component...
	@cd src/XrdSfs;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdRootd: FORCE
	@echo $(XMSG) rootd component...
	@cd src/XrdRootd;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdSec: FORCE
	@echo $(XMSG) sec component...
	@cd src/XrdSec;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdSeckrb4: FORCE
	@echo $(XMSG) seckrb4 component...
	@cd src/XrdSeckrb4;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdSeckrb5: FORCE
	@echo $(XMSG) seckrb5 component...
	@cd src/XrdSeckrb5;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

Xrd: FORCE
	@echo $(XMSG) xrd component...
	@cd src/Xrd;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdXr: FORCE
	@echo $(XMSG) XrdXr component...
	@cd src/XrdXr;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

XrdXrootd: FORCE
	@echo $(XMSG) xrootd component...
	@cd src/XrdXrootd;\
	$(MAKE) $(MAKEXEQ) ARCH=$(ARCH) --no-print-directory

FORCE: ;
