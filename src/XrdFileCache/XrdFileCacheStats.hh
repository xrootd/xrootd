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
   //! Disk-based cache statistics.
   //----------------------------------------------------------------------------
   class Stats : public XrdOucCacheStats
   {
      public:
         //------------------------------------------------------------------------
         //! Constructor
         //------------------------------------------------------------------------
         Stats() :
         m_BytesCachedPrefetch(0),
         m_BytesPrefetch(0),
         m_HitsPrefetch(0),
         m_HitsDisk(0) {}

         long long m_BytesCachedPrefetch; //!< bytes already prefetch
         long long m_BytesPrefetch;       //!< bytes waited
         int       m_HitsPrefetch;        //!< blocks already prefetched
         int       m_HitsDisk;            //!< blocks waited

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
