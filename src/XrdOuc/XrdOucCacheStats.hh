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
   in for each CacheIO and each Cache object. The former usually identifies
   a specific file while the latter provides summary information.
*/

class XrdOucCacheStats
{
public:
long long    BytesPead;  // Bytes read via preread (not included in BytesRead)
long long    BytesRead;  // Total number of bytes read into the cache
long long    BytesGet;   // Number of bytes delivered from the cache
long long    BytesPass;  // Number of bytes read but not cached
long long    BytesWrite; // Total number of bytes written from the cache
long long    BytesPut;   // Number of bytes updated in the cache
int          Hits;       // Number of times wanted data was in the cache
int          Miss;       // Number of times wanted data was *not* in the cache
int          HitsPR;     // Number of pages wanted data was just preread
int          MissPR;     // Number of pages wanted data was just    read

inline void Get(XrdOucCacheStats &Dst)
               {sMutex.Lock();
                Dst.BytesRead   = BytesPead;  Dst.BytesGet    = BytesRead;
                Dst.BytesPass   = BytesPass;
                Dst.BytesWrite  = BytesWrite; Dst.BytesPut    = BytesPut;
                Dst.Hits        = Hits;       Dst.Miss        = Miss;
                Dst.HitsPR      = HitsPR;     Dst.MissPR      = MissPR;
                sMutex.UnLock();
               }

inline void Add(XrdOucCacheStats &Src)
               {sMutex.Lock();
                BytesRead  += Src.BytesPead;  BytesGet   += Src.BytesRead;
                BytesPass  += Src.BytesPass;
                BytesWrite += Src.BytesWrite; BytesPut   += Src.BytesPut;
                Hits       += Src.Hits;       Miss       += Src.Miss;
                HitsPR     += Src.HitsPR;     MissPR     += Src.MissPR;
                sMutex.UnLock();
               }

inline void  Add(long long &Dest, int &Val)
                {sMutex.Lock(); Dest += Val; sMutex.UnLock();}

inline void  Lock()   {sMutex.Lock();}
inline void  UnLock() {sMutex.UnLock();}

             XrdOucCacheStats() : BytesPead(0), BytesRead(0),  BytesGet(0),
                                  BytesPass(0), BytesWrite(0), BytesPut(0),
                                  Hits(0),      Miss(0),
                                  HitsPR(0),    MissPR(0) {}
            ~XrdOucCacheStats() {}
private:
XrdSysMutex sMutex;
};
#endif
