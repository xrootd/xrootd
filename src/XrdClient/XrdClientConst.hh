//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientConst                                                             //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Constants for Xrd                                                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef _XRC_CONST_H
#define _XRC_CONST_H

#define DFLT_CONNECTTIMEOUT     2
#define NAME_CONNECTTIMEOUT     "ConnectTimeout"

#define DFLT_CONNECTTIMEOUTWAN  2
#define NAME_CONNECTTIMEOUTWAN  "ConnectTimeoutWan"

#define DFLT_REQUESTTIMEOUT     5
#define NAME_REQUESTTIMEOUT     "RequestTimeout"


#define DFLT_MAXREDIRECTCOUNT   10
#define NAME_MAXREDIRECTCOUNT   "MaxRedirectcount"

#define DFLT_DEBUG              2
#define NAME_DEBUG              "DebugLevel"

#define DFLT_RECONNECTTIMEOUT   10
#define NAME_RECONNECTTIMEOUT   "ReconnectTimeout"

#define DFLT_REDIRCNTTIMEOUT	3600
#define NAME_REDIRCNTTIMEOUT    "RedirCntTimeout"

#define DFLT_FIRSTCONNECTMAXCNT 10
#define NAME_FIRSTCONNECTMAXCNT "FirstConnectMaxCnt"

#define TXSOCK_ERR_TIMEOUT	-1
#define TXSOCK_ERR		-2

// Maybe we don't want to start the garbage collector
// But the default must be to start it
#define DFLT_STARTGARBAGECOLLECTORTHREAD  1
#define NAME_STARTGARBAGECOLLECTORTHREAD  "StartGarbageCollectorThread"

#define DFLT_GOASYNC 1
#define NAME_GOASYNC            "GoAsync"

#define  XRD_CLIENT_VERSION "0.1.0alpha"

// Defaults for ReadAhead and Cache
#define DFLT_READCACHESIZE      3000000
#define NAME_READCACHESIZE      "ReadCacheSize"

#define DFLT_READAHEADSIZE      500000
#define NAME_READAHEADSIZE      "ReadAheadSize"

#define NAME_REDIRDOMAINALLOW_RE   "RedirDomainAllowRE"
#define NAME_REDIRDOMAINDENY_RE    "RedirDomainDenyRE"
#define NAME_CONNECTDOMAINALLOW_RE "ConnectDomainAllowRE"
#define NAME_CONNECTDOMAINDENY_RE  "ConnectDomainDenyRE"

#define PROTO "root"

#define TRUE  1
#define FALSE 0

#define SafeDelete(x) if (x) delete x;

#endif

