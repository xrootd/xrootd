#ifndef __SUT_CACHE_H__
#define __SUT_CACHE_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d S u t C a c h e . h h                           */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XProtocol/XPtypes.hh"
#include "XrdSut/XrdSutPFEntry.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                                                                            */
/*  For caching temporary information during the authentication handshake     */
/*                                                                            */
/******************************************************************************/

class XrdSutPFCacheRef
{
public:

inline void Lock(XrdSysMutex *Mutex)
                {if (mtx) {if (mtx != Mutex) mtx->UnLock();
                              else return;
                          }
                 Mutex->Lock();
                 mtx = Mutex;
                };

inline void Set(XrdSysMutex *Mutex)
               {if (mtx) {if (mtx != Mutex) mtx->UnLock();
                             else return;
                         }
                mtx = Mutex;
               };

inline void UnLock() {if (mtx) {mtx->UnLock(); mtx = 0;}}

            XrdSutPFCacheRef() : mtx(0) {}

           ~XrdSutPFCacheRef() {if (mtx) UnLock();}
protected:
XrdSysMutex *mtx;
};

class XrdSutPFCache
{
private:
   XrdSysRWLock    rwlock;  // Access synchronizator
   int             cachesz; // Number of entries allocated
   int             cachemx; // Largest Index of allocated entries
   XrdSutPFEntry **cachent; // Pointers to filled entries
   kXR_int32       utime;   // time at which was last updated
   int             lifetime; // lifetime (in secs) of the cache info 
   XrdOucHash<kXR_int32> hashtable; // Reflects the file index structure
   kXR_int32       htmtime;   // time at which hash table was last rebuild
   XrdOucString    pfile;   // file name (if loaded from file)
   bool            isinit;  // true if already initialized

   XrdSutPFEntry  *Get(const char *ID, bool *wild);
   bool            Delete(XrdSutPFEntry *pfEnt);

   static const int maxTries = 100; // Max time to try getting a lock
   static const int retryMSW = 300; // Milliseconds to wait to get lock

public:
   XrdSutPFCache() { cachemx = -1; cachesz = 0; cachent = 0; lifetime = 300;
                   utime = -1; htmtime = -1; pfile = ""; isinit = 0; }
   virtual ~XrdSutPFCache();

   // Status
   int            Entries() const { return (cachemx+1); }
   bool           Empty() const { return (cachemx == -1); }

   // Initialization methods
   int            Init(int capacity = 100, bool lock = 1);
   int            Reset(int newsz = -1, bool lock = 1);
   int            Load(const char *pfname);  // build cache of a pwd file
   int            Flush(const char *pfname = 0);   // flush content to pwd file
   int            Refresh();    // refresh content from source file
   int            Rehash(bool force = 0, bool lock = 1);  // (re)build hash table
   void           SetLifetime(int lifet = 300) { lifetime = lifet; }

   // Cache management
   XrdSutPFEntry *Get(int i) const { return (i<=cachemx) ? cachent[i] :
                                                          (XrdSutPFEntry *)0; }
   XrdSutPFEntry *Get(XrdSutPFCacheRef &urRef, const char *ID, bool *wild = 0);
   XrdSutPFEntry *Add(XrdSutPFCacheRef &urRef, const char *ID, bool force = 0);
   bool           Remove(const char *ID, int opt = 1);
   int            Trim(int lifet = 0);

   // For debug purposes
   void           Dump(const char *msg= 0);
};

#endif

