#######################################################################
## Makefile.am for xrootd
##
##
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

bin_PROGRAMS = cmsd

xrootdcmsincdir = $(includedir)/xrootd/XrdCms

xrootdcmsinc_HEADERS = \
    XrdCmsAdmin.hh        XrdCmsCache.hh        XrdCmsClient.hh       \
    XrdCmsClientConfig.hh XrdCmsClientMan.hh    XrdCmsClientMsg.hh    \
    XrdCmsCluster.hh      XrdCmsConfig.hh       XrdCmsFinder.hh       \
    XrdCmsJob.hh          XrdCmsLogin.hh        XrdCmsManList.hh      \
    XrdCmsManTree.hh      XrdCmsManager.hh      XrdCmsMeter.hh        \
                          XrdCmsNode.hh         XrdCmsPList.hh        \
    XrdCmsParser.hh       XrdCmsPrepArgs.hh     XrdCmsPrepare.hh      \
    XrdCmsProtocol.hh     XrdCmsRRData.hh       XrdCmsRRQ.hh          \
    XrdCmsRTable.hh       XrdCmsReq.hh          XrdCmsResp.hh         \
    XrdCmsRouting.hh      XrdCmsSelect.hh       XrdCmsState.hh        \
    XrdCmsSupervisor.hh   XrdCmsTrace.hh        XrdCmsTypes.hh        \
    XrdCmsXmi.hh          XrdCmsXmiReq.hh


INCLUDES = -I$(top_srcdir)/src

cmsd_SOURCES = \
XrdCmsAdmin.cc \
XrdCmsCache.cc \
XrdCmsCluster.cc \
XrdCmsConfig.cc \
XrdCmsJob.cc \
XrdCmsManList.cc \
XrdCmsManTree.cc \
XrdCmsManager.cc \
XrdCmsMeter.cc \
XrdCmsNode.cc \
XrdCmsPList.cc \
XrdCmsPrepArgs.cc \
XrdCmsPrepare.cc \
XrdCmsProtocol.cc \
XrdCmsReq.cc \
XrdCmsRouting.cc \
XrdCmsRRQ.cc \
XrdCmsRTable.cc \
XrdCmsState.cc \
XrdCmsSupervisor.cc \
XrdCmsXmiReq.cc \
XrdCmsLogin.cc \
XrdCmsParser.cc \
XrdCmsRRData.cc

#    XrdCmsAdmin.cc        XrdCmsCache.cc        XrdCmsClientConfig.cc \
#    XrdCmsClientMan.cc    XrdCmsClientMsg.cc    XrdCmsCluster.cc      \
#    XrdCmsConfig.cc       XrdCmsFinder.cc       XrdCmsJob.cc          \
#    XrdCmsLogin.cc        XrdCmsManList.cc      XrdCmsManTree.cc      \
#    XrdCmsManager.cc      XrdCmsMeter.cc                              \
#    XrdCmsNode.cc         XrdCmsPList.cc        XrdCmsParser.cc       \
#    XrdCmsPrepArgs.cc     XrdCmsPrepare.cc      XrdCmsProtocol.cc     \
#    XrdCmsRRData.cc       XrdCmsRRQ.cc          XrdCmsRTable.cc       \
#    XrdCmsReq.cc          XrdCmsResp.cc         XrdCmsRouting.cc      \
#    XrdCmsState.cc        XrdCmsSupervisor.cc   XrdCmsXmiReq.cc

cmsd_LDADD = ../XrdNet/libXrdNet.la \
             ../XrdOuc/libXrdOuc.la \
             ../XrdSys/libXrdSys.la \
             ../Xrd/libXrd.la \
             -ldl

cmsd_LDFLAGS = -static -export-dynamic


