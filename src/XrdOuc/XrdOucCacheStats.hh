#ifndef __XRDOUCCACHESTATS_HH__
#define __XRDOUCCACHESTATS_HH__
/******************************************************************************/
/*                                                                            */
/*                   X r d O u c C a c h e S t a t s . h h                    */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdint>
#include <cstring>

#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysPthread.hh"

/* The XrdOucCacheStats object holds statistics on cache usage. It is available
   in each Cache object that records the summary information for that cache.
*/

class XrdOucCacheStats
{
public:

struct CacheStats
{
// General read/write information
//
long long  BytesPead;   // Bytes read via preread (not included in BytesRead)
long long  BytesRead;   // Total number of bytes read into the cache
long long  BytesGet;    // Number of bytes delivered from the cache
long long  BytesPass;   // Number of bytes read but not cached
long long  BytesWrite;  // Total number of bytes written from the cache
long long  BytesPut;    // Number of bytes updated in the cache
long long  BytesSaved;  // Number of bytes written from memory to disk
long long  BytesPurged; // Number of bytes purged from the cache
long long  Hits;        // Number of times wanted data was in the cache
long long  Miss;        // Number of times wanted data was *not* in the cache
long long  Pass;        // Number of times wanted data was read but not cached
long long  HitsPR;      // Number of pages of   wanted data was just preread
long long  MissPR;      // Number of pages of unwanted data was just preread
 
// Local file information
//
long long  FilesOpened; // Number of cache files opened
long long  FilesClosed; // Number of cache files closed
long long  FilesCreated;// Number of cache files created
long long  FilesPurged; // Number of cache files purged (i.e. deleted)
long long  FilesInCache;// Number of      files currently in the cache
long long  FilesAreFull;// Number of full files currently in the cache
 
// Permanent storage information (all state information)
//
long long  DiskSize;    // Size of disk cache in bytes
long long  DiskUsed;    // Size of disk cache in use (bytes)
long long  DiskMin;     // Minimum bytes that were in use
long long  DiskMax;     // Maximum bytes that were in use
 
// Memory information (all state information)
//
long long  MemSize;     // Maximum bytes that can be in memory
long long  MemUsed;     // Actual bytes that are allocated in memory
long long  MemWriteQ;   // Actual bytes that are in write queue
 
// File information (supplied by the POSIX layer)
//
long long  OpenDefers;  // Number of opens  that were deferred
long long  DeferOpens;  // Number of defers that were actually opened
long long  ClosDefers;  // Number of closes that were deferred
long long  ClosedLost;  // Number of closed file objects that were lost
}          X;           // This must be a POD type

inline void Get(XrdOucCacheStats &D)
               {sMutex.Lock();
                memcpy(&D.X, &X, sizeof(CacheStats));
                sMutex.UnLock();
               }

inline void Add(XrdOucCacheStats &S)
               {sMutex.Lock();
                X.BytesPead   += S.X.BytesPead;   X.BytesRead  += S.X.BytesRead;
                X.BytesGet    += S.X.BytesGet;    X.BytesPass  += S.X.BytesPass;
                X.BytesSaved  += S.X.BytesSaved;  X.BytesPurged+= S.X.BytesPurged;
/* R/W Cache */ X.BytesWrite  += S.X.BytesWrite;  X.BytesPut   += S.X.BytesPut;
                X.Hits        += S.X.Hits;        X.Miss       += S.X.Miss;
                X.Pass        += S.X.Pass;
                X.HitsPR      += S.X.HitsPR;      X.MissPR     += S.X.MissPR;
                sMutex.UnLock();
               }

inline void Set(XrdOucCacheStats &S)
               {sMutex.Lock();
                X.FilesOpened  = S.X.FilesOpened; X.FilesClosed = S.X.FilesClosed;
                X.FilesCreated = S.X.FilesCreated;X.FilesPurged = S.X.FilesPurged;
                X.FilesInCache = S.X.FilesInCache;X.FilesAreFull= S.X.FilesAreFull;

                X.DiskSize     = S.X.DiskSize;    X.DiskUsed    = S.X.DiskUsed;
                X.DiskMin      = S.X.DiskMin;     X.DiskMax     = S.X.DiskMax;

                X.MemSize      = S.X.MemSize;     X.MemUsed     = S.X.MemUsed;
                X.MemWriteQ    = S.X.MemWriteQ;
                sMutex.UnLock();
               }

inline void  Add(long long &Dest, long long Val)
                {sMutex.Lock(); Dest += Val; sMutex.UnLock();}

inline void  Count(long long &Dest)
                  {AtomicBeg(sMutex); AtomicInc(Dest); AtomicEnd(sMutex);}

inline void  Set(long long &Dest, long long Val)
                {sMutex.Lock(); Dest  = Val; sMutex.UnLock();}

inline void  Lock()   {sMutex.Lock();}
inline void  UnLock() {sMutex.UnLock();}

             XrdOucCacheStats() {memset(&X, 0, sizeof(CacheStats));}
            ~XrdOucCacheStats() {}
private:
XrdSysMutex sMutex;
};
#endif
