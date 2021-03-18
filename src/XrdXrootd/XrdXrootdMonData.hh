#ifndef __XRDXROOTDMONDATA__
#define __XRDXROOTDMONDATA__
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d M o n D a t a . h h                    */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include "XProtocol/XPtypes.hh"

/******************************************************************************/
/*                    P a c k e t   D e f i n i t i o n s                     */
/******************************************************************************/
  
struct XrdXrootdMonHeader
       {kXR_char   code;         // '='|'d'|'f'|'i'|'p'|'r'|'t'|'u'|'x'
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

struct XrdXrootdMonRedir
      {union   {kXR_int32 Window;
       struct  {kXR_char  Type;
                kXR_char  Dent;
                kXR_int16 Port;
               }          rdr;     } arg0;
       union   {kXR_unt32 dictid;
                kXR_int32 Window;  } arg1;
      };

struct XrdXrootdMonBurr
       {XrdXrootdMonHeader hdr;
        union {kXR_int64   sID;
               kXR_char    sXX[8]; };
        XrdXrootdMonRedir  info[sizeof(XrdXrootdMonRedir)]; //This is really [n]
       };

struct XrdXrootdMonGS
      {XrdXrootdMonHeader hdr;
       int                tBeg;     // time(0) of the first record
       int                tEnd;     // time(0) of the last  record
       kXR_int64          sID;      // Server id in lower 48 bits
};                                  // Information provider top 8 bits.

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
const kXR_char XROOTD_MON_READU         = 0x91;
const kXR_char XROOTD_MON_REDHOST       = 0xf0; // No   Modifier
const kXR_char XROOTD_MON_WINDOW        = 0xe0;


const kXR_char XROOTD_MON_MAPIDNT       = '=';
const kXR_char XROOTD_MON_MAPPATH       = 'd';
const kXR_char XROOTD_MON_MAPFSTA       = 'f'; // The "f" stream
const kXR_char XROOTD_MON_MAPGSTA       = 'g'; // The "g" stream
const kXR_char XROOTD_MON_MAPINFO       = 'i';
const kXR_char XROOTD_MON_MAPMIGR       = 'm'; // Internal use only!
const kXR_char XROOTD_MON_MAPPURG       = 'p';
const kXR_char XROOTD_MON_MAPREDR       = 'r';
const kXR_char XROOTD_MON_MAPSTAG       = 's'; // Internal use only!
const kXR_char XROOTD_MON_MAPTRCE       = 't';
const kXR_char XROOTD_MON_MAPUSER       = 'u';
const kXR_char XROOTD_MON_MAPXFER       = 'x';

const kXR_char XROOTD_MON_GSCCM         = 'M'; // pfc: Cache context mgt info
const kXR_char XROOTD_MON_GSPFC         = 'C'; // pfc: Cache monitoring  info
const kXR_char XROOTD_MON_GSTCP         = 'T'; // TCP connection statistics

// The following bits are insert in the low order 4 bits of the MON_REDIRECT
// entry code to indicate the actual operation that was requestded.
//
const kXR_char XROOTD_MON_REDSID        = 0xf0; // Server Identification
const kXR_char XROOTD_MON_REDTIME       = 0x00; // Timing mark

const kXR_char XROOTD_MON_REDIRECT      = 0x80; // With Modifier below!
const kXR_char XROOTD_MON_REDLOCAL      = 0x90; // With Modifier below!

const kXR_char XROOTD_MON_CHMOD         = 0x01; // Modifiers for the above
const kXR_char XROOTD_MON_LOCATE        = 0x02;
const kXR_char XROOTD_MON_OPENDIR       = 0x03;
const kXR_char XROOTD_MON_OPENC         = 0x04;
const kXR_char XROOTD_MON_OPENR         = 0x05;
const kXR_char XROOTD_MON_OPENW         = 0x06;
const kXR_char XROOTD_MON_MKDIR         = 0x07;
const kXR_char XROOTD_MON_MV            = 0x08;
const kXR_char XROOTD_MON_PREP          = 0x09;
const kXR_char XROOTD_MON_QUERY         = 0x0a;
const kXR_char XROOTD_MON_RM            = 0x0b;
const kXR_char XROOTD_MON_RMDIR         = 0x0c;
const kXR_char XROOTD_MON_STAT          = 0x0d;
const kXR_char XROOTD_MON_TRUNC         = 0x0e;

const kXR_char XROOTD_MON_FORCED        = 0x01;
const kXR_char XROOTD_MON_BOUNDP        = 0x02;

const int      XROOTD_MON_REDMASK       = 0x00000ff;
const int      XROOTD_MON_SRCMASK       = 0x000000f;
const int      XROOTD_MON_TRGMASK       = 0x7fffff0;
const int      XROOTD_MON_NEWSTID       = 0x8000000;

const long long XROOTD_MON_SIDMASK      = 0x0000ffffffffffff;
const long long XROOTD_MON_PIDMASK      = 0xff;
const long long XROOTD_MON_PIDSHFT      = 56;

/******************************************************************************/
/*           " f "   S t r e a m   S p e c i f i c   R e c o r d s            */
/******************************************************************************/

// The UDP buffer layout is as follows:
//
// XrdXrootdMonHeader    with Code    ==  XROOTD_MON_MAPFSTA
// XrdXrootdMonFileTOD   with recType == isTime
// XrdXrootdMonFileHdr   with recType == one of recTval   (variable length)
// ...                   additional XrdXrootdMonFileHdr's (variable length)
// XrdXrootdMonFileTOD   with recType == isTime
  
struct XrdXrootdMonFileHdr    // 8
{
enum  recTval {isClose = 0,   // Record for close
               isOpen,        // Record for open
               isTime,        // Record for time
               isXfr,         // Record for transfers
               isDisc         // Record for disconnection
              };

enum  recFval {forced  =0x01, // If recFlag == isClose close due to disconnect
               hasOPS  =0x02, // If recFlag == isClose MonStatXFR + MonStatOPS
               hasSSQ  =0x04, // If recFlag == isClose XFR + OPS  + MonStatSSQ
               hasCSE  =0x04, // If recFlag == isClose XFR + OPS  + MonStatSSQ
               hasLFN  =0x01, // If recFlag == isOpen  the lfn is present
               hasRW   =0x02, // If recFlag == isOpen  file opened r/w
               hasSID  =0x01  // if recFlag == isTime sID is present (new rec)
              };

char      recType;  // RecTval: isClose | isOpen | isTime | isXfr
char      recFlag;  // RecFval: Record type-specific flags
short     recSize;  // Size of this record in bytes
union
{
kXR_unt32 fileID;   // dictid  of file for all rectypes except "disc" & "time"
kXR_unt32 userID;   // dictid  of user for     rectypes equal  "disc"
short     nRecs[2]; // isTime: nRecs[0] == isXfr recs nRecs[1] == total recs
};
};

// The following record is always be present as the first record in the udp
// udp packet and should be used to establish the recording window.
//
struct XrdXrootdMonFileTOD
{
XrdXrootdMonFileHdr Hdr;      //  8
int                 tBeg;     // time(0) of following record
int                 tEnd;     // time(0) when packet was sent
kXR_int64           sID;      // Server id in lower 48 bits
};


// The following variable length structure exists in XrdXrootdMonFileOPN if
// "lfn" has been specified. It exists only when recFlag & hasLFN is TRUE.
// The user's dictid will be zero (missing) if user monitoring is not enabled.
//
struct XrdXrootdMonFileLFN
{
kXR_unt32           user;     // Monitoring dictid for the user, may be 0.
char                lfn[1028];// Variable length, use recSize!
};

// The following is reported when a file is opened. If "lfn" was specified and
// Hdr.recFlag & hasLFN is TRUE the XrdXrootdMonFileLFN structure is present.
// However, it variable in size and the next record will be found using recSize.
// The lfn is gauranteed to end with at least one null byte.
//
struct XrdXrootdMonFileOPN
{
XrdXrootdMonFileHdr Hdr;      //  8
long long           fsz;      //  8 file size at time of open
XrdXrootdMonFileLFN ufn;      //  Present ONLY if recFlag & hasLFN is TRUE
};

// The following data is collected on a per file basis
//
struct XrdXrootdMonStatPRW    //  8 Bytes
{
long long           rBytes;   // Bytes read  from file so far using pgread()
int                 rCount;   // Number of operations
int                 rRetry;   // Number of pgread  retries (pages)
long long           wBytes;   // Bytes written to file so far using pgwrite()
int                 wCount;   // Number of operations
int                 wRetry;   // Number of pgwrite retries (corrections)
int                 wcsErr;   // Number of pgwrite checksum errors
int                 wcsUnc;   // Number of pgwrite uncorrected checksums
};

struct XrdXrootdMonStatOPS    // 48 Bytes
{
int                 read;     // Number of read()  calls
int                 readv;    // Number of readv() calls
int                 write;    // Number of write() calls
short               rsMin;    // Smallest  readv() segment count
short               rsMax;    // Largest   readv() segment count
long long           rsegs;    // Number of readv() segments
int                 rdMin;    // Smallest  read()  request size
int                 rdMax;    // Largest   read()  request size
int                 rvMin;    // Smallest  readv() request size
int                 rvMax;    // Largest   readv() request size
int                 wrMin;    // Smallest  write() request size
int                 wrMax;    // Largest   write() request size
};

union XrdXrootdMonDouble
{
long long           dlong;
double              dreal;
};

struct XrdXrootdMonStatSSQ    // 32 Bytes (all values net ordered IEEE754)
{
XrdXrootdMonDouble  read;     // Sum (all read  requests)**2 (size)
XrdXrootdMonDouble  readv;    // Sum (all readv requests)**2 (size  as a unit)
XrdXrootdMonDouble  rsegs;    // Sum (all readv segments)**2 (count as a unit)
XrdXrootdMonDouble  write;    // Sum (all write requests)**2 (size)
};

// The following transfer data is collected for each open file.
//
struct XrdXrootdMonStatXFR
{
long long           read;     // Bytes read  from file so far using read()
long long           readv;    // Bytes read  from file so far using readv()
long long           write;    // Bytes written to file so far
};

// The following is reported upon file close. This is a variable length record.
// The record always contains XrdXrootdMonStatXFR after   XrdXrootdMonFileHdr.
// If (recFlag & hasOPS) TRUE XrdXrootdMonStatOPS follows XrdXrootdMonStatXFR
// If (recFlag & hasSSQ) TRUE XrdXrootdMonStatSQV follows XrdXrootdMonStatOPS
// The XrdXrootdMonStatSSQ information is present only if "ssq" was specified.
//
struct XrdXrootdMonFileCLS    // 32 | 80 | 96 Bytes
{
XrdXrootdMonFileHdr Hdr;      // Always present (recSize has full length)
XrdXrootdMonStatXFR Xfr;      // Always present
XrdXrootdMonStatOPS Ops;      // Only   present when (recFlag & hasOPS) is True
XrdXrootdMonStatSSQ Ssq;      // Only   present when (recFlag & hasSSQ) is True
};

// The following is reported when a user ends a session.
//
struct XrdXrootdMonFileDSC
{
XrdXrootdMonFileHdr Hdr;      //  8
};

// The following is reported each interval*count for each open file when "xfr"
// is specified. These records may be interspersed with other records.
//
struct XrdXrootdMonFileXFR    // 32 Bytes
{
XrdXrootdMonFileHdr Hdr;      // Always present with recType == isXFR
XrdXrootdMonStatXFR Xfr;      // Always present
};
#endif
