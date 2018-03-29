#ifndef  __SUT_CACHEENTRY_H
#define  __SUT_CACHEENTRY_H
/******************************************************************************/
/*                                                                            */
/*                 X r d S u t C a c h e E n t r y . h h                      */
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
/*  Class defining the basic cache entry                                      */
/*                                                                            */
/******************************************************************************/

enum kCEntryStatus {
   kCE_inactive = -2,     // -2  inactive: eliminated at next trim
   kCE_disabled,          // -1  disabled, cannot be enabled
   kCE_allowed,           //  0  empty creds, can be enabled 
   kCE_expired,           //  1  enabled, creds to be changed at next used
   kCE_ok,                //  2  enabled and OK
   kCE_special            //  3  special (non-creds) entry
};

//
// Buffer used internally by XrdGCEntry
//
class XrdSutCacheEntryBuf {
public:
   char      *buf;
   kXR_int32  len;
   XrdSutCacheEntryBuf(char *b = 0, kXR_int32 l = 0);
   XrdSutCacheEntryBuf(const XrdSutCacheEntryBuf &b);

   virtual ~XrdSutCacheEntryBuf() { if (len > 0 && buf) delete[] buf; }

   void SetBuf(const char *b = 0, kXR_int32 l = 0);
};

//
// Generic cache entry: it stores a
//
//        name
//        status                     2 bytes
//        cnt                        2 bytes
//        mtime                      4 bytes
//        buf1, buf2, buf3, buf4
//
// The buffers are generic buffers to store bufferized info
//
class XrdSutCacheEntry {
public:
   char        *name;
   short        status;
   short        cnt;            // counter
   kXR_int32    mtime;          // time of last modification / creation
   XrdSutCacheEntryBuf    buf1;
   XrdSutCacheEntryBuf    buf2;
   XrdSutCacheEntryBuf    buf3;
   XrdSutCacheEntryBuf    buf4;
   XrdSysRWLock rwmtx;      // Locked when reference is outstanding
   XrdSutCacheEntry(const char *n = 0, short st = 0, short cn = 0,
                 kXR_int32 mt = 0);
   XrdSutCacheEntry(const XrdSutCacheEntry &e);
   virtual ~XrdSutCacheEntry() { if (name) delete[] name; }
   kXR_int32 Length() const { return (buf1.len + buf2.len + 2*sizeof(short) +
                                      buf3.len + buf4.len + 5*sizeof(kXR_int32)); }
   void Reset();
   void SetName(const char *n = 0);
   char *AsString() const;

   XrdSutCacheEntry &operator=(const XrdSutCacheEntry &pfe);
};

class XrdSutCERef
{
public:

inline void ReadLock(XrdSysRWLock *lock = 0)
                { if (lock) Set(lock);
                  rwlock->ReadLock();
                };

inline void WriteLock(XrdSysRWLock *lock = 0)
                { if (lock) Set(lock);
                  rwlock->WriteLock();
                };

inline void Set(XrdSysRWLock *lock)
               {if (rwlock) {if (rwlock != lock) rwlock->UnLock();
                             else return;
                         }
                rwlock = lock;
               };

inline void UnLock(bool reset = true) {if (rwlock) {rwlock->UnLock(); if (reset) rwlock = 0; }}

            XrdSutCERef() : rwlock(0) {}

           ~XrdSutCERef() {if (rwlock) UnLock(); rwlock = 0; }
protected:
XrdSysRWLock *rwlock;
};

#endif
