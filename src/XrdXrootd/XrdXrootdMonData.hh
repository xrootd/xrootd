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
       {kXR_char   code;         // '='|'d'|'i'|'p'|'r'|'t'|'u'|'x'
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
const kXR_char XROOTD_MON_REDHOST       = 0xf0; // No   Modifier
const kXR_char XROOTD_MON_WINDOW        = 0xe0;


const kXR_char XROOTD_MON_MAPIDNT       = '=';
const kXR_char XROOTD_MON_MAPPATH       = 'd';
const kXR_char XROOTD_MON_MAPINFO       = 'i';
const kXR_char XROOTD_MON_MAPMIGR       = 'm'; // Internal use only!
const kXR_char XROOTD_MON_MAPPURG       = 'p';
const kXR_char XROOTD_MON_MAPREDR       = 'r';
const kXR_char XROOTD_MON_MAPSTAG       = 's'; // Internal use only!
const kXR_char XROOTD_MON_MAPTRCE       = 't';
const kXR_char XROOTD_MON_MAPUSER       = 'u';
const kXR_char XROOTD_MON_MAPXFER       = 'x';

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

const int      XROOTD_MON_SRCMASK       = 0x000000f;
const int      XROOTD_MON_TRGMASK       = 0x7fffff0;
const int      XROOTD_MON_NEWSTID       = 0x8000000;

#endif
