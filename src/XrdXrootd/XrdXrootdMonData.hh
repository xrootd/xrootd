#ifndef __XRDXROOTDMONDATA__
#define __XRDXROOTDMONDATA__
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d M o n D a t a . h h                    */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//       $Id$

#include "XProtocol/XPtypes.hh"

/******************************************************************************/
/*                    P a c k e t   D e f i n i t i o n s                     */
/******************************************************************************/
  
struct XrdXrootdMonHeader
       {kXR_char   code;         // 'd' | 'i' | 't'
        kXR_char   pseq;         // packet sequence
        kXR_int16  plen;         // packet length
        kXR_int32  stod;         // Unix time at Server Start
       };

struct XrdXrootdMonTrace
       {union {union {kXR_int64  val;
                      kXR_char   id[8];
                      kXR_unt32  rTot[2]; } arg0;
               union {kXR_int32  buflen;
                      kXR_int32  Window;
                      kXR_unt32  wTot;    } arg1;
               union {kXR_unt32  dictid;
                      kXR_int32  Window;  } arg2;

               kXR_char rec[16];
              };
       };

struct XrdXrootdMonBuff
       {XrdXrootdMonHeader hdr;
        XrdXrootdMonTrace  info[1];    // Actually this is [n]
       };

struct XrdXrootdMonMap
       {XrdXrootdMonHeader hdr;
        kXR_unt32          dictid;
        char               info[1024+256];
       };
  
#define XROOTD_MON_APPID  0xa0
#define XROOTD_MON_CLOSE  0xc0
#define XROOTD_MON_OPEN   0x80
#define XROOTD_MON_WINDOW 0xe0

#define XROOTD_MON_MAPPATH 'd'
#define XROOTD_MON_MAPINFO 'i'

#endif
