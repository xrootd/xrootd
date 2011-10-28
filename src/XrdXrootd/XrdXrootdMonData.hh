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

#include "XProtocol/XPtypes.hh"

/******************************************************************************/
/*                    P a c k e t   D e f i n i t i o n s                     */
/******************************************************************************/
  
struct XrdXrootdMonHeader
       {kXR_char   code;         // 'd' | 'i' | 'm' | 'r' | 's' | 't' | 'u'
        kXR_char   pseq;         // packet sequence
        kXR_unt16  plen;         // packet length
        kXR_int32  stod;         // Unix time at Server Start
       };

struct XrdXrootdMonTrace
       {union {kXR_int64  val;
               kXR_char   id[8];
               kXR_unt16  sVal[4];
               kXR_unt32  rTot[2]; } arg0;
        union {kXR_int32  buflen;
               kXR_int32  Window;
               kXR_unt32  wTot;    } arg1;
        union {kXR_unt32  dictid;
               kXR_int32  Window;  } arg2;
       };

struct XrdXrootdMonBuff
       {XrdXrootdMonHeader hdr;
        XrdXrootdMonTrace  info[sizeof(XrdXrootdMonTrace)]; //This is really [n]
       };

struct XrdXrootdMonMap
       {XrdXrootdMonHeader hdr;
        kXR_unt32          dictid;
        char               info[1024+256];
       };
  
const kXR_char XROOTD_MON_APPID         = 0xa0;
const kXR_char XROOTD_MON_CLOSE         = 0xc0;
const kXR_char XROOTD_MON_DISC          = 0xd0;
const kXR_char XROOTD_MON_OPEN          = 0x80;
const kXR_char XROOTD_MON_READV         = 0x90;
const kXR_char XROOTD_MON_WINDOW        = 0xe0;

const kXR_char XROOTD_MON_MAPPATH       = 'd';
const kXR_char XROOTD_MON_MAPINFO       = 'i';
const kXR_char XROOTD_MON_MAPMIGR       = 'm';
const kXR_char XROOTD_MON_MAPPURG       = 'p';
const kXR_char XROOTD_MON_MAPREDR       = 'r';
const kXR_char XROOTD_MON_MAPSTAG       = 's';
const kXR_char XROOTD_MON_MAPTRCE       = 't';
const kXR_char XROOTD_MON_MAPUSER       = 'u';

const kXR_char XROOTD_MON_FORCED        = 0x01;
const kXR_char XROOTD_MON_BOUNDP        = 0x02;

#endif
