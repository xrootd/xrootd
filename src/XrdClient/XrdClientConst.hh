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

#define DFLT_CONNECTTIMEOUT    2
#define DFLT_CONNECTTIMEOUTWAN 2
#define DFLT_REQUESTTIMEOUT    5

#define DFLT_MAXREDIRECTCOUNT        10
#define DFLT_DEBUG                   2
#define DFLT_RECONNECTTIMEOUT        10

#define DFLT_REDIRCNTTIMEOUT		     3600
#define DFLT_FIRSTCONNECTMAXCNT   10

#define TXSOCK_ERR_TIMEOUT	-1
#define TXSOCK_ERR		-2

// Maybe we don't want to start the garbage collector
// But the default must be to start it
#define DFLT_STARTGARBAGECOLLECTORTHREAD	1

#define DFLT_GOASYNC 1

#define  XRD_CLIENT_VERSION "0.1.0alpha"

// Defaults for ReadAhead and Cache
#define DFLT_READCACHESIZE 3000000
#define DFLT_READAHEADSIZE 500000


#define PROTO "root"

#define TRUE  1
#define FALSE 0

#define SafeDelete(x) if (x) delete x;

#endif

