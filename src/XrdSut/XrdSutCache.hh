#ifndef  __SUT_CACHE_H
#define  __SUT_CACHE_H
/******************************************************************************/
/*                                                                            */
/*                       X r d S u t C a c h e . h h                          */
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

#include "XrdOuc/XrdOucHash.hh"
#include "XrdSut/XrdSutCacheEntry.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                                                                            */
/*  Class defining the basic memory cache                                     */
/*                                                                            */
/******************************************************************************/

typedef bool (*XrdSutCacheGet_t)(XrdSutCacheEntry *, void *);
typedef struct {
   long arg1;
   long arg2;
   long arg3;
   long arg4;
} XrdSutCacheArg_t;

class XrdSutCache {
public:
   XrdSutCache(int psize = 89, int size = 144, int load = 80) : table(psize, size, load) {}
   virtual ~XrdSutCache() {}

   XrdSutCacheEntry *Get(const char *tag) {
      // Get the entry with 'tag'.
      // If found the entry is returned rd-locked.
      // If rd-locking fails the status is set to kCE_inactive.
      // Returns null if not found.

      XrdSutCacheEntry *cent = 0;

      // Exclusive access to the table
      XrdSysMutexHelper raii(mtx);

      // Look for an entry
      if (!(cent = table.Find(tag))) {
         // none found
         return cent;
      }

      // We found an existing entry:
      // lock until we get the ability to read (another thread may be valudating it)
      int status = 0;
      cent->rwmtx.ReadLock( status );
      if ( status ) {
         // A problem occurred: fail (set the entry invalid)
         cent->status = kCE_inactive;
      }
      return cent;
   }

   XrdSutCacheEntry *Get(const char *tag, bool &rdlock, XrdSutCacheGet_t condition = 0, void *arg = 0) {
      // Get or create the entry with 'tag'.
      // New entries are always returned write-locked.
      // The status of existing ones depends on condition: if condition is undefined or if applied
      // to the entry with arguments 'arg' returns true, the entry is returned read-locked.
      // Otherwise a write-lock is attempted on the entry: if unsuccessful (another thread is modifing
      // the entry) the entry is read-locked.
      // The status of the lock is returned in rdlock (true if read-locked).
      rdlock = false;
      XrdSutCacheEntry *cent = 0;

      // Exclusive access to the table
      XrdSysMutexHelper raii(mtx);

      // Look for an entry
      if (!(cent = table.Find(tag))) {
         // If none, create a new one and write-lock for validation
         cent = new XrdSutCacheEntry(tag);
         int status = 0;
         cent->rwmtx.WriteLock( status );
         if (status) {
            // A problem occurred: delete the entry and fail
            delete cent;
            return (XrdSutCacheEntry *)0;
         }
         // Register it in the table
         table.Add(tag, cent);
         return cent;
      }

      // We found an existing entry:
      // lock until we get the ability to read (another thread may be valudating it)
      int status = 0;
      cent->rwmtx.ReadLock( status );
      if (status) {
         // A problem occurred: fail (set the entry invalid)
         cent->status = kCE_inactive;
         return cent;
      }

      // Check-it by apply the condition, if required
      if (condition) {
         if ((*condition)(cent, arg)) {
            // Good and valid entry
            rdlock = true;
         } else {
            // Invalid entry: unlock and write-lock to be able to validate it
            cent->rwmtx.UnLock();
            int status = 0;
            cent->rwmtx.WriteLock( status );
            if (status) {
               // A problem occurred: fail (set the entry invalid)
               cent->status = kCE_inactive;
               return cent;
            }
          }
      } else {
          // Good and valid entry
          rdlock = true;
      }
      // We are done: return read-locked so we can use it until we need it
      return cent;
   }

   inline int Num() { return table.Num(); }
   inline void Reset() { return table.Purge(); }

private:
   XrdSysRecMutex         mtx;  // Protect access to table
   XrdOucHash<XrdSutCacheEntry> table; // table with content
};

#endif
