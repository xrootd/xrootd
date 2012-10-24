#ifndef  __SUT_PFENTRY_H
#define  __SUT_PFENTRY_H
/******************************************************************************/
/*                                                                            */
/*                      X r d S u t P F E n t r y . h h                       */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Gerri Ganis for CERN                                         */
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

#include "XProtocol/XProtocol.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                                                                            */
/*  Class defining the basic entry into a PFile                               */
/*                                                                            */
/******************************************************************************/

enum kPFEntryStatus {
   kPFE_inactive = -2,     // -2  inactive: eliminated at next trim
   kPFE_disabled,          // -1  disabled, cannot be enabled
   kPFE_allowed,           //  0  empty creds, can be enabled 
   kPFE_ok,                //  1  enabled and OK
   kPFE_onetime,           //  2  enabled, can be used only once
   kPFE_expired,           //  3  enabled, creds to be changed at next used
   kPFE_special,           //  4  special (non-creds) entry
   kPFE_anonymous,         //  5  enabled, OK, no creds, counter
   kPFE_crypt              //  6  enabled, OK, crypt-like credentials
};

//
// Buffer used internally by XrdSutPFEntry
//
class XrdSutPFBuf {
public:
   char      *buf;
   kXR_int32  len;   
   XrdSutPFBuf(char *b = 0, kXR_int32 l = 0);
   XrdSutPFBuf(const XrdSutPFBuf &b);

   virtual ~XrdSutPFBuf() { if (len > 0 && buf) delete[] buf; }

   void SetBuf(const char *b = 0, kXR_int32 l = 0);
};

//
// Generic File entry: it stores a 
//
//        name
//        status                     2 bytes
//        cnt                        2 bytes
//        mtime                      4 bytes
//        buf1, buf2, buf3, buf4
//
// The buffers are generic buffers to store bufferized info
//
class XrdSutPFEntry {
public:
   char        *name;
   short        status;
   short        cnt;            // counter
   kXR_int32    mtime;          // time of last modification / creation
   XrdSutPFBuf  buf1;
   XrdSutPFBuf  buf2;
   XrdSutPFBuf  buf3;
   XrdSutPFBuf  buf4;
   XrdSysMutex  pfeMutex;      // Locked when reference is outstanding
   XrdSutPFEntry(const char *n = 0, short st = 0, short cn = 0,
                 kXR_int32 mt = 0);
   XrdSutPFEntry(const XrdSutPFEntry &e);
   virtual ~XrdSutPFEntry() { if (name) delete[] name; } 
   kXR_int32 Length() const { return (buf1.len + buf2.len + 2*sizeof(short) +
                                      buf3.len + buf4.len + 5*sizeof(kXR_int32)); }
   void Reset();
   void SetName(const char *n = 0);
   char *AsString() const;

   XrdSutPFEntry &operator=(const XrdSutPFEntry &pfe);
};

#endif
