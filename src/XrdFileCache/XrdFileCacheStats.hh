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
#include "XrdSys/XrdSysPthread.hh"

namespace XrdFileCache
{
   //----------------------------------------------------------------------------
   //! Statistics of disk cache utilisation.
   //----------------------------------------------------------------------------
   class Stats : public XrdOucCacheStats
   {
      public:
         //----------------------------------------------------------------------
         //! Constructor.
         //----------------------------------------------------------------------
         Stats() :
         m_BytesCachedPrefetch(0),
         m_BytesPrefetch(0),
         m_HitsPrefetch(0),
         m_HitsDisk(0) {}

         long long m_BytesCached;  //!< number of bytes served from cache
         long long m_BytesRemote;  //!< number of bytes that had to be fetched
         int       m_HitsCached;   //!< number of read requests served from cache
         int       m_HitsPartial;  //!< number of read requests that were partially available
         int       m_HitsRemote;   //!< number of read requests that had to be fetched

      // int       m_HitsPartial[10]; 
      // [0] 0-10%, [1] 10-20% .... [9] 90-100%
      // int       m_HitsPartial[12];
      // [0] - 0;   [1] 0-10%, [2] 10-20% .... [10] 90-100%; [11] 100%

         inline void AddStat(Stats &Src)
         {
            XrdOucCacheStats::Add(Src);

            m_MutexXfc.Lock();
            m_BytesCachedPrefetch += Src.m_BytesCachedPrefetch;
            m_BytesPrefetch       += Src.m_BytesPrefetch;

            m_HitsPrefetch += Src.m_HitsPrefetch;
            m_HitsDisk     += Src.m_HitsDisk;

            m_MutexXfc.UnLock();
         }

      private:
         XrdSysMutex m_MutexXfc;
   };
}

#endif
