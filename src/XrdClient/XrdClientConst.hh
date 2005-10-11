//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientConst                                                       //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Constants for Xrd                                                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#ifndef _XRC_CONST_H
#define _XRC_CONST_H

#define DFLT_CONNECTTIMEOUT     60
#define NAME_CONNECTTIMEOUT     (char *)"ConnectTimeout"

#define DFLT_CONNECTTIMEOUTWAN  60
#define NAME_CONNECTTIMEOUTWAN  (char *)"ConnectTimeoutWan"

#define DFLT_REQUESTTIMEOUT     60
#define NAME_REQUESTTIMEOUT     (char *)"RequestTimeout"


#define DFLT_MAXREDIRECTCOUNT   255
#define NAME_MAXREDIRECTCOUNT   (char *)"MaxRedirectcount"

#define DFLT_DEBUG              0
#define NAME_DEBUG              (char *)"DebugLevel"

#define DFLT_RECONNECTTIMEOUT   20
#define NAME_RECONNECTTIMEOUT   (char *)"ReconnectTimeout"

#define DFLT_REDIRCNTTIMEOUT	3600
#define NAME_REDIRCNTTIMEOUT    (char *)"RedirCntTimeout"

#define DFLT_FIRSTCONNECTMAXCNT 150
#define NAME_FIRSTCONNECTMAXCNT (char *)"FirstConnectMaxCnt"

#define TXSOCK_ERR_TIMEOUT	-1
#define TXSOCK_ERR		-2

// Maybe we don't want to start the garbage collector
// But the default must be to start it
#define DFLT_STARTGARBAGECOLLECTORTHREAD  1
#define NAME_STARTGARBAGECOLLECTORTHREAD  (char *)"StartGarbageCollectorThread"

#define DFLT_GOASYNC 1
#define NAME_GOASYNC            (char *)"GoAsync"

// keep/dont-keep the socket open (required by optimized rootd fallback)
#define DFLT_KEEPSOCKOPENIFNOTXRD 0
#define NAME_KEEPSOCKOPENIFNOTXRD (char *)"KeepSockOpenIfNotXrd"

// Printable version
#define XRD_CLIENT_VERSION      (char *)"0.3"

// Version and capabilities sent to the server
#define XRD_CLIENT_CURRENTVER   1
#define XRD_CLIENT_CAPVER       ((kXR_char)kXR_asyncap | XRD_CLIENT_CURRENTVER)

// Defaults for ReadAhead and Cache
#define DFLT_READCACHESIZE      (3*1024*1024)
#define NAME_READCACHESIZE      (char *)"ReadCacheSize"

#define DFLT_READAHEADSIZE      262144
#define NAME_READAHEADSIZE      (char *)"ReadAheadSize"

// Read ahead style: 0=sync, 1=enable async if possible
#define DFLT_READAHEADTYPE     0
#define NAME_READAHEADTYPE     (char *)"ReadAheadType"

#define NAME_REDIRDOMAINALLOW_RE   (char *)"RedirDomainAllowRE"
#define NAME_REDIRDOMAINDENY_RE    (char *)"RedirDomainDenyRE"
#define NAME_CONNECTDOMAINALLOW_RE (char *)"ConnectDomainAllowRE"
#define NAME_CONNECTDOMAINDENY_RE  (char *)"ConnectDomainDenyRE"

#define PROTO (char *)"root"

// The max number of threads spawned to do parallel opens
#define DFLT_MAXCONCURRENTOPENS    500

#define TRUE  1
#define FALSE 0

#define xrdmin(a, b) (a < b ? a : b)
#define xrdmax(a, b) (a > b ? a : b)

#endif

