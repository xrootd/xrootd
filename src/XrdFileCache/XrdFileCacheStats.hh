#ifndef __XRDFILECACHE_STATS_HH__
#define __XRDFILECACHE_STATS_HH__

//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University  
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman           
//----------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------------------

#include <XrdOuc/XrdOucCache.hh>

/******************************************************************************/
/*                     C l a s s   Stats                      */
/******************************************************************************/
/*
the class is owned by prefetch

in general XrdOucCacheStats could be used but we need addtional information 
what is task-wait downloaded and what is already prefecthed

the statistios is later used in IO::Detach , where it is appended to the global XrdFileCache statistics.

 */

namespace XrdFileCache
{
class Stats : public XrdOucCacheStats
{
public:
    long long    BytesCachedPrefetch;
    long long    BytesPrefetch;
    long long    BytesDisk;
    int          HitsPrefetch;
    int          HitsDisk;
    time_t       AppendTime;

    inline void AddStat(Stats &Src)
    {
        XrdOucCacheStats::Add(Src);

        //  sMutex1.Lock();
        BytesCachedPrefetch += Src.BytesCachedPrefetch;
        BytesPrefetch       += Src.BytesPrefetch;
        BytesDisk           += Src.BytesDisk;

        HitsPrefetch += Src.HitsPrefetch;
        HitsDisk     += Src.HitsDisk;

        // sMutex1.UnLock();
    }

    Stats() :
        BytesCachedPrefetch(0), 
        BytesPrefetch(0),
        BytesDisk(0),
        HitsPrefetch(0), 
        HitsDisk(0) {
        AppendTime = time(0);
    }

   void Dump() const;

private:    
   //   XrdSysMutex sMutex1;

};
}

#endif
