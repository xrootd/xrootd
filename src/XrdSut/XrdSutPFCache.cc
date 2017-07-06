/******************************************************************************/
/*                                                                            */
/*                      X r d S u t C a c h e . c c                           */
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "XrdSut/XrdSutPFCache.hh"
#include "XrdSut/XrdSutPFile.hh"
#include "XrdSut/XrdSutTrace.hh"
#include "XrdSut/XrdSutAux.hh"
#include "XrdSys/XrdSysTimer.hh"

/******************************************************************************/
/*                                                                            */
/*  For caching temporary information during the authentication handshake     */
/*                                                                            */
/******************************************************************************/

//__________________________________________________________________
XrdSutPFCache::~XrdSutPFCache()
{
   // Destructor

   // We are destroying the cache 
   rwlock.WriteLock();
   
   // Cleanup content
   while (cachemx > -1) {
      if (cachent[cachemx]) {
         delete cachent[cachemx];
         cachent[cachemx] = 0;
      }
      cachemx--;
   }
   // Cleanup table
   if (cachent)
      delete[] cachent;
   
   // Done
   rwlock.UnLock();
}

//__________________________________________________________________
int XrdSutPFCache::Init(int capacity, bool lock)
{
   // Initialize the cache to hold up to capacity entries.
   // Later on, capacity is double each time more space is needed. 
   // Return 0 if ok, -1 otherwise
   EPNAME("Cache::Init");

   // Lock for writing
   if (lock) rwlock.WriteLock();

   // Nothing to do if already done
   if (isinit) {
      if (lock) rwlock.UnLock();
      return 0;
   }

   // Make sure capacity makes sense; use a default, if not
   capacity = (capacity > 0) ? capacity : 100;

   // Allocate
   cachent = new XrdSutPFEntry *[capacity];
   if (cachent) {
      for (int i = 0; i < capacity; i++) { cachent[i] = 0; }
      cachesz = capacity;
      DEBUG("cache allocated for "<<cachesz<<" entries");

      // Update time stamp
      utime = (kXR_int32)time(0);
      
      // Init hash table
      if (Rehash(0, 0) != 0) {
         DEBUG("problems initialising hash table");
      }
      // UnLock
      if (lock) rwlock.UnLock();
      return 0;

   } else
      DEBUG("could not allocate cache - out-of-resources ");

   // Flag has initialized
   isinit = 1;

   // UnLock
   if (lock) rwlock.UnLock();
   return -1;
}

//__________________________________________________________________
XrdSutPFEntry *XrdSutPFCache::Get(XrdSutPFCacheRef &urRef, const char *ID, bool *wild)
{
   // Retrieve an entry with ID, if any
   // If wild is defined, search also best matching regular expression
   // with wildcard '*'; *wild = 0 will indicate exact match, 
   // *wild = 1 wild card compatibility match 
   EPNAME("Cache::Get");
   XrdSutPFEntry *pfEnt;
   int i;

   TRACE(Dump,"locating entry for ID: "<<ID);

   //
   // If ID is undefined, do nothing
   if (!ID || !strlen(ID)) {
      DEBUG("empty ID !");
      return (XrdSutPFEntry *)0 ;
   }
   if (wild) *wild = 0;

   if (Rehash() != 0) {
      DEBUG("problems rehashing");
      return (XrdSutPFEntry *)0 ;
   }

   // Lock for reading
   XrdSysRWLockHelper isg(rwlock, 1);

   // Find the entry and lock it. Repeat if we can get a lock.
   //
   for (i = 0; i < maxTries; i++)
       {if ((pfEnt = Get(ID, wild)))
           {if (pfEnt->pfeMutex.CondLock())
               {urRef.Set(&(pfEnt->pfeMutex));
                return pfEnt;
               }
           } else return pfEnt;
        isg.UnLock();
        XrdSysTimer::Wait(retryMSW);
        if (Rehash() != 0)
           {DEBUG("problems rehashing");
            return (XrdSutPFEntry *)0 ;
           }
        isg.Lock(&rwlock, 1);
       }

   // Nothing found
   return (XrdSutPFEntry *)0 ;
}

//__________________________________________________________________
XrdSutPFEntry *XrdSutPFCache::Get(const char *ID, bool *wild)
{

   // Look in the hash first
   kXR_int32 *ie = hashtable.Find(ID);
   if (ie && *ie >= 0 && *ie < cachesz) {
      // Return the associated entry
      return cachent[*ie];
   }

   // If wild cards allowed search sequentially
   if (wild) {
      XrdOucString sid(ID);
      int i = 0, match = 0, nmmax = 0, iref = -1;
      for (; i <= cachemx; i++) {
         if (cachent[i]) {
            match = sid.matches(cachent[i]->name);
            if (match > nmmax) { 
               nmmax = match;
               iref = i;
            }
         }
      }
      if (iref > -1) {
         *wild = 1;
         return cachent[iref];
      }
   }

   // Nothing found
   return (XrdSutPFEntry *)0 ;
}

//__________________________________________________________________
XrdSutPFEntry *XrdSutPFCache::Add(XrdSutPFCacheRef &urRef, const char *ID, bool force)
{
   // Add an entry with ID in cache
   // Cache buffer is re-allocated with double size, if needed
   // Hash is updated
   EPNAME("Cache::Add");

   //
   // IF ID is undefined, do nothing
   if (!ID || !strlen(ID)) {
      DEBUG("empty ID !");
      return (XrdSutPFEntry *)0 ;
   }

   //
   // If an entry already exists, return it
   XrdSutPFEntry *ent = Get(urRef, ID);
   if (ent)
      return ent;

   // Lock for writing
   XrdSysRWLockHelper isg(rwlock, 0);

   //
   // Make sure there enough space for a new entry
   if (cachemx == cachesz - 1) {
      //
      // Duplicate buffer
      XrdSutPFEntry **newcache = new XrdSutPFEntry *[2*cachesz];
      if (!newcache) {
         DEBUG("could not extend cache to size: "<<(2*cachesz));
         return (XrdSutPFEntry *)0 ;
      }
      // Update info
      cachesz *= 2;
      //
      // Copy existing valid entries, calculating real size
      int i = 0, nmx = 0;
      for (; i <= cachemx; i++) {
         if (cachent[i]) {
            newcache[nmx] = cachent[i];
            nmx++;
         }
      }
      // update size
      cachemx = nmx - 1;
      //
      // Reset new entries
      for (i = cachemx + 1; i <= cachemx; i++) {
         newcache[i] = 0;
      }
      //
      // Cleanup and reassign
      delete[] cachent;
      cachent = newcache;
      //
      // Force rehash in this case
      force = 1;
   }
   //
   // The next free
   int pos = cachemx + 1;

   //
   // Add new entry
   cachent[pos] = new XrdSutPFEntry(ID);   
   if (cachent[pos]) {
      cachemx = pos;
   } else {
      DEBUG("could not allocate space for new cache entry");
      return (XrdSutPFEntry *)0 ;
   }
   // Update time stamp
   utime = (kXR_int32)time(0);

   // Rebuild hash table
   if (Rehash(force, 0) != 0) {
      DEBUG("problems re-hashing");
      return (XrdSutPFEntry *)0 ;
   }

   // We are done (we can lock the entry without a wait)
   urRef.Lock(&(cachent[pos]->pfeMutex));
   return cachent[pos];
}

//__________________________________________________________________
bool XrdSutPFCache::Remove(const char *ID, int opt)
{
   // If opt==1 remove entry with name matching exactly ID from cache
   // If opt==0 all entries with names starting with ID are removed
   // Return 1 if ok, 0 otherwise
   EPNAME("Cache::Remove");

   //
   // IF ID is undefined, do nothing
   if (!ID || !strlen(ID)) {
      DEBUG("empty ID !");
      return 0 ;
   }

   // Lock for writing
   XrdSysRWLockHelper isg(rwlock, 0);

   if (Rehash(0, 0) != 0) {
      DEBUG("problems rehashing");
      return 0 ;
   }

   bool found = 0;
   if (opt == 1) {
      int pos = -1;
      // Look in the hash first
      kXR_int32 *ie = hashtable.Find(ID);
      if (*ie >= 0 && *ie < cachesz) {
         // Return the associated entry
         pos = *ie;
      }
      
      //
      // Check if pos makes sense
      if (cachent[pos] && !strcmp(cachent[pos]->name,ID)) {
         if (!Delete(cachent[pos])) DEBUG("Delete defered for " <<ID);
         cachent[pos] = 0;
         // We are done, if not the one at highest index
         if (pos < cachemx)
            return 1;
         // We update the highest index
         found = 1;
      }
   } else {
      // Loop over entries
      int i = cachemx;
      for (; i >= 0; i--) {
         if (cachent[i]) {
            if (!strncmp(cachent[i]->name,ID,strlen(ID))) {
               if (!Delete(cachent[i])) DEBUG("Delete defered for " <<ID);
               cachent[i] = 0;
               found = 1;
            }
         }
      }
   }

   if (found) {
      // Update time stamp
      utime = (kXR_int32)time(0);
      
      // Rebuild hash table
      if (Rehash(0, 0) != 0) {
         DEBUG("problems re-hashing");
         return 0 ;
      }
   }

   // We are done
   return found;
}

//__________________________________________________________________
bool XrdSutPFCache::Delete(XrdSutPFEntry *pfEnt)
{
   struct pfQ {pfQ           *next;
               XrdSutPFEntry *pfEnt;
                              pfQ(pfQ *cP, XrdSutPFEntry *tP)
                                 : next(cP), pfEnt(tP) {}
                             ~pfQ() {delete pfEnt;}
              };
   EPNAME("Cache::Delete");
   static pfQ pfDefer(0,0);

// Try to remove all defered entries first
//
   if (pfDefer.next)
      {pfQ *pQ = &pfDefer, *dQ;
       int nTot = 0, dTot = 0;
       while((dQ = pQ->next))
            {nTot++;
             if (dQ->pfEnt->pfeMutex.CondLock())
                {pQ->next = dQ->next;
                 dQ->pfEnt->pfeMutex.UnLock();
                 delete dQ;
                 dTot++;
                } else pQ = dQ;
            }
       if (nTot) DEBUG("Defered delete " <<dTot <<" of " <<nTot);
      }

// Now try to delete this entry
//
   if (pfEnt->pfeMutex.CondLock())
      {pfEnt->pfeMutex.UnLock();
       delete pfEnt;
       return true;
      }

// Defer the delete as someone still has a reference to the entry
//
   pfDefer.next = new pfQ(pfDefer.next, pfEnt);
   return false;
}

//__________________________________________________________________
int XrdSutPFCache::Trim(int lifet)
{
   // Remove entries older then lifet seconds. If lifet <=0, compare
   // to lifetime, which can be set with SetValidity().
   // Return number of entries removed

   // Lock for writing
   EPNAME("Cache::Trim");
   XrdSysRWLockHelper isg(rwlock, 0);

   //
   // Make sure lifet makes sense; if not, use internal default
   lifet = (lifet > 0) ? lifet : lifetime;

   //
   // Reference time
   int reftime = time(0) - lifet;

   // Loop over entries
   int i = cachemx, nrm = 0;
   for (; i >= 0; i--) {
      if (cachent[i] && cachent[i]->mtime < reftime) {
         if (!Delete(cachent[i]))
            DEBUG("Delete defered for " <<cachent[i]->name);
         cachent[i] = 0;
         nrm++;
      }
      if (i == cachemx) {
         if (!cachent[i])
            cachemx--;
      }
   }

   // We are done
   return nrm;
}

//__________________________________________________________________
int XrdSutPFCache::Reset(int newsz, bool lock)
{
   // Remove all existing entries.
   // If newsz > -1, set new capacity to newsz, reallocating if needed
   // Return 0 if ok, -1 if problems reallocating.
   EPNAME("Cache::Reset");

   // Lock for writing
   if (lock) rwlock.WriteLock();

   // Loop over entries
   int i = cachemx;
   for (; i >= 0; i--) {
      if (cachent[i]) {
         if (!Delete(cachent[i]))
            DEBUG("Delete defered for " <<cachent[i]->name);
         cachent[i] = 0;
      }
   }

   int rc = 0;
   // Reallocate, if requested
   if (newsz > -1 && newsz != cachesz) {
      delete[] cachent;
      cachent = 0;
      cachesz = 0;      
      cachemx = -1;
      isinit = 0;
      rc = Init(newsz, 0);
   }

   // Unlock
   if (lock) rwlock.UnLock();

   // We are done
   return rc;
}

//________________________________________________________________
void XrdSutPFCache::Dump(const char *msg)
{
   // Dump content of the cache
   EPNAME("Cache::Dump");

   PRINT("//-----------------------------------------------------");
   PRINT("//");
   if (msg && strlen(msg) > 0) {
      PRINT("// "<<msg);
      PRINT("//");
   }
   PRINT("//  Capacity:         "<<cachesz);
   PRINT("//  Max index filled: "<<cachemx);
   PRINT("//");

   // Lock for reading
   XrdSysRWLockHelper isg(rwlock, 1);

   if (cachesz > 0) {

      XrdSutPFEntry *ent = 0;
      int i = 0, nn = 0;
      for (; i <= cachemx; i++) {

         // get entry
         if ((ent = cachent[i])) {

            char smt[20] = {0};
            XrdSutTimeString(ent->mtime,smt);
               
            nn++;
            PRINT("// #:"<<nn<<"  st:"<<ent->status<<" cn:"<<ent->cnt
                  <<"  buf:"<<ent->buf1.len<<","<<ent->buf2.len<<","
                  <<ent->buf3.len<<","<<ent->buf4.len<<" mod:"<<smt
                  <<" name:"<<ent->name);
         }

      }
      PRINT("//");
   }
   PRINT("//-----------------------------------------------------");
}

//__________________________________________________________________
int XrdSutPFCache::Load(const char *pfn)
{
   // Initialize the cache from the content of a file of PF entries
   // Return 0 if ok, -1 otherwise
   EPNAME("Cache::Load");

   // Make sure file name is defined
   if (!pfn) {
      DEBUG("invalid input file name");
      return -1;
   }

   // Check if file exists and if it has been modified since last load
   struct stat st;
   if (stat(pfn,&st) == -1) {
      DEBUG("cannot stat file (errno: "<<errno<<")");
      return -1;
   }
   if (utime > -1 && utime > st.st_mtime) {
      DEBUG("cached information for file "<<pfn<<" is up-to-date");
      return 0;
   }

   // Lock for writing
   XrdSysRWLockHelper isg(rwlock, 0);

   // Attach to file and open it
   XrdSutPFile ff(pfn, kPFEopen);
   if (!ff.IsValid()) {
      DEBUG("file is not a valid PFEntry file ("<<ff.LastErrStr()<<")");
      return -1;
   }

   // Read the header
   XrdSutPFHeader header;
   if (ff.ReadHeader(header) < 0) {
      ff.Close();
      return -1;
   }

   // If the file has no entries there is nothing to do
   if (header.entries <= 0) {
      DEBUG("PFEntry file is empty - default init and return");
      // Save file name
      pfile = pfn;
      Init(-1, 0);
      return 0;
   }

   // Allocate cache, if not done already or if too small
   if (Reset(header.entries, 0) == -1) {
      DEBUG("problems allocating / resizing cache ");
      ff.Close();
      return -1;
   }

   // Read entries
   kXR_int32 ne = 0;
   XrdSutPFEntInd ind;
   kXR_int32 nxtofs = header.indofs;
   while (nxtofs > 0 && ne < header.entries) {
      //
      // read index entry
      if (ff.ReadInd(nxtofs, ind) < 0) {
         DEBUG("problems reading index entry ");
         ff.Close();
         return -1;
      }

      // If active ...
      if (ind.entofs > 0) {

         // Read entry out
         XrdSutPFEntry ent;
         if (ff.ReadEnt(ind.entofs, ent) < 0) {
            ff.Close();
            return -1;
         }

         // Copy for the cache
         XrdSutPFEntry *cent = new XrdSutPFEntry(ent);

         if (cent) {
            // Set the id
            cent->SetName(ind.name);
            
            // Fill the array 
            cachent[ne] = cent;

            // Count
            ne++;

         } else {
            DEBUG("problems duplicating entry for cache");
            ff.Close();
            return -1;
         }
      }

      // Go to next
      nxtofs = ind.nxtofs;
   }
   cachemx = ne-1;
   if (nxtofs > 0)
      DEBUG("WARNING: inconsistent number of entries: possible file corruption");

   // Update the time stamp
   utime = (kXR_int32)time(0);

   // Save file name
   pfile = pfn;

   // Close the file
   ff.Close();

   DEBUG("PF file "<<pfn<<" loaded in cache (found "<<ne<<" entries)");

   // Force update hash table
   if (Rehash(1, 0) != 0) {
      DEBUG("problems creating hash table");
      return -1;
   }

   return 0;
}


//__________________________________________________________________
int XrdSutPFCache::Rehash(bool force, bool lock)
{
   // Update or create hahs table corresponding to the present content of the
   // cache
   // Return 0 if ok, -1 otherwise
   EPNAME("Cache::Rehash");

   // Lock for writing
   if (lock) rwlock.WriteLock();

   if (htmtime >= utime && !force) {
      TRACE(Dump, "hash table is up-to-date");
      if (lock) rwlock.UnLock();
      return 0;
   }

   // Clean up the hash table
   hashtable.Purge();

   kXR_int32 i = 0, nht = 0;
   for (; i <= cachemx; i++) {
      if (cachent[i]) {
         // Fill the hash table 
         kXR_int32 *key = new kXR_int32(i);
         if (key) {
            TRACE(Dump, "Adding ID: "<<cachent[i]->name<<"; key: "<<*key);
            hashtable.Add(cachent[i]->name,key);
            nht++;
         }
      }
   }
   // Update modification time
   htmtime = (kXR_int32)time(0);

   // Unlock
   if (lock) rwlock.UnLock();

   DEBUG("Hash table updated (found "<<nht<<" active entries)");
   return 0;
}

//__________________________________________________________________
int XrdSutPFCache::Flush(const char *pfn)
{
   // Flush cache content to file pfn.
   // If pfn == 0 and the cache was initialized from a file, flush
   // to initializing file.
   // If pfn does not exist, create it.
   // Return 0 if ok, -1 otherwise
   EPNAME("Cache::Flush");

   // Make sure we have all the info
   if (!pfn && pfile.length() <= 0) {
      DEBUG("invalid input");
      return -1;
   }
   if (!pfn)
      pfn = pfile.c_str();

   // Attach to file and open it; create if not ther
   XrdSutPFile ff(pfn, (kPFEopen | kPFEcreate));
   if (!ff.IsValid()) {
      DEBUG("cannot attach-to or create file "<<pfn<<" ("<<ff.LastErrStr()<<")");
      return -1;
   }

   // Lock for writing
   XrdSysRWLockHelper isg(rwlock, 0);

   //
   // Loop over cache entries
   int i = 0, nr = 0, nfs = 0;
   for (; i <= cachemx; i++ ) {
      if (cachent[i]) {
         //
         // Retrieve related from file, if any
         // Read entry out
         XrdSutPFEntry ent;
         if ((nr = ff.ReadEntry(cachent[i]->name, ent)) < 0) {
            ff.Close();
            return -1;
         }
         //
         // Write (update) only if older that cache or not found
         if (nr == 0 || cachent[i]->mtime > ent.mtime) {         
            if (ff.WriteEntry(*cachent[i]) < 0) {
               ff.Close();
               return -1;
            }
            nfs++;
         }
      }
   }

   // Close the file
   ff.Close();

   // Update the time stamp (to avoid fake loads later on)
   utime = (kXR_int32)time(0);

   // Save file name
   if (pfile.length() <= 0)
      pfile = pfn;

   DEBUG("Cache flushed to file "<<pfn<<" ("<<nfs<<" entries updated / written)");

   return 0;
}

//__________________________________________________________________
int XrdSutPFCache::Refresh()
{
   // Refresh content of a cache created from file.
    // Return 0 if ok, -1 otherwise
   EPNAME("Cache::Refresh");

   // Make sure we have all the info
   if (pfile.length() <= 0) {
      DEBUG("cache was not initialized from file - do nothing");
      return -1;
   }

   // Check if file exists and if it has been modified since last load
   struct stat st;
   if (stat(pfile.c_str(),&st) == -1) {
      DEBUG("cannot stat file (errno: "<<errno<<")");
      return -1;
   }
   if (utime > -1 && utime > st.st_mtime) {
      DEBUG("cached information for file "<<pfile<<" is up-to-date");
      return 0;
   }

   // Lock for writing
   XrdSysRWLockHelper isg(rwlock, 0);

   if (Load(pfile.c_str()) != 0) {
      DEBUG("problems loading passwd information from file: "<<pfile);
      return -1;
   }

   // Update the time stamp (to avoid fake loads or refreshs later on)
   utime = (kXR_int32)time(0);

   DEBUG("Cache refreshed from file: "<<pfile);

   return 0;
}


