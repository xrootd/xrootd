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

#include "XrdSys/XrdSysPthread.hh"

/* The XrdOucCacheStats object holds statistics on cache usage. It is available
   in each Cache object that records the summary information for that cache.
*/

class XrdOucCacheStats
{
public:
long long    BytesPead;  // Bytes read via preread (not included in BytesRead)
long long    BytesRead;  // Bytes read into the cache excluding prereads.
long long    BytesGet;   // Bytes delivered from the cache (disk or memory)
long long    BytesPass;  // Bytes read but not cached (bypass from origin)
long long    BytesSaved; // Bytes written from the memory cache to disk
long long    BytesPurge; // Bytes purged  from the disk   cache

long long    BytesWrite; // Bytes written from the cache to origin
long long    BytesPut;   // Bytes updated in the cache due to writes

long long    Hits;       // Number of times wanted data was in the cache
long long    Miss;       // Number of times wanted data was *not* in the cache
long long    HitsPR;     // Number of pages wanted data was just preread
long long    MissPR;     // Number of pages wanted data was just    read

long long    DiskSize;   // Maximum bytes that can be on disk (usually set once)
long long    DiskUsed;   // Actual  bytes that are    on disk
long long    MemoSize;   // Maximum bytes that can be in memory
long long    MemoUsed;   // Actual  bytes that are    in memory

inline void Get(XrdOucCacheStats &Dst)
               {sMutex.Lock();
                Dst.BytesPead   = BytesPead;  Dst.BytesRead   = BytesRead;
                Dst.BytesGet    = BytesGet;   Dst.BytesPass   = BytesPass;
                Dst.BytesSaved  = BytesSaved; Dst.BytesPurge  = BytesPurge;
/* R/W Cache */ Dst.BytesWrite  = BytesWrite; Dst.BytesPut    = BytesPut;
                Dst.Hits        = Hits;       Dst.Miss        = Miss;
                Dst.HitsPR      = HitsPR;     Dst.MissPR      = MissPR;
                Dst.DiskSize    = DiskSize;   Dst.DiskUsed    = DiskUsed;
                Dst.MemoSize    = MemoSize;   Dst.MemoUsed    = MemoUsed;
                sMutex.UnLock();
               }

inline void Add(XrdOucCacheStats &Src)
               {sMutex.Lock();
                BytesPead  += Src.BytesPead;  BytesRead  += Src.BytesRead;
                BytesGet   += Src.BytesGet;   BytesPass  += Src.BytesPass;
                BytesSaved += Src.BytesSaved; BytesPurge += Src.BytesPurge;
/* R/W Cache */ BytesWrite += Src.BytesWrite; BytesPut   += Src.BytesPut;
                Hits       += Src.Hits;       Miss       += Src.Miss;
                HitsPR     += Src.HitsPR;     MissPR     += Src.MissPR;
                DiskSize    = Src.DiskSize;   DiskUsed    = Src.DiskUsed;
                MemoSize    = Src.MemoSize;   MemoUsed    = Src.MemoUsed;
                sMutex.UnLock();
               }

inline void  Add(long long &Dest, int &Val)
                {sMutex.Lock(); Dest += Val; sMutex.UnLock();}

inline void  Set(long long &Dest, int &Val)
                {sMutex.Lock(); Dest  = Val; sMutex.UnLock();}

inline void  Lock()   {sMutex.Lock();}
inline void  UnLock() {sMutex.UnLock();}

             XrdOucCacheStats() : BytesPead(0), BytesRead(0),  BytesGet(0),
                                  BytesPass(0), BytesSaved(0), BytesPurge(0),
                                  BytesWrite(0),BytesPut(0),
                                  Hits(0),      Miss(0),
                                  HitsPR(0),    MissPR(0),
                                  DiskSize(0),  DiskUsed(0),
                                  MemoSize(0),  MemoUsed(0) {}
            ~XrdOucCacheStats() {}
private:
XrdSysMutex sMutex;
};
#endif
