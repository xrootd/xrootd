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

#include "XrdOuc/XrdOucCache.hh"
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
         Stats() {
            m_BytesDisk = m_BytesRam = m_BytesMissed = 0;
         }

         long long m_BytesDisk;   //!< number of bytes served from disk cache
         long long m_BytesRam;    //!< number of bytes served from RAM cache
         long long m_BytesMissed; //!< number of bytes served directly from XrdCl

         inline void AddStat(Stats &Src)
         {
            XrdOucCacheStats::Add(Src);

            m_MutexXfc.Lock();
            m_BytesDisk += Src.m_BytesDisk;
            m_BytesRam += Src.m_BytesRam;
            m_BytesMissed += Src.m_BytesMissed;

            m_MutexXfc.UnLock();
         }

      private:
         XrdSysMutex m_MutexXfc;
   };
}

#endif
