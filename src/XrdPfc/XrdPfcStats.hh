#ifndef __XRDPFC_STATS_HH__
#define __XRDPFC_STATS_HH__

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

namespace XrdPfc
{
//----------------------------------------------------------------------------
//! Statistics of cache utilisation by a File object.
//----------------------------------------------------------------------------
class Stats
{
public:
   int       m_NumIos;          //!< number of IO objects attached during this access
   int       m_Duration;        //!< total duration of all IOs attached
   long long m_BytesHit;        //!< number of bytes served from disk
   long long m_BytesMissed;     //!< number of bytes served from remote and cached
   long long m_BytesBypassed;   //!< number of bytes served directly through XrdCl
   long long m_BytesWritten;    //!< number of bytes written to disk
   int       m_NCksumErrors;    //!< number of checksum errors while getting data from remote

   //----------------------------------------------------------------------

   Stats() :
      m_NumIos  (0), m_Duration(0),
      m_BytesHit(0), m_BytesMissed(0), m_BytesBypassed(0),
      m_BytesWritten(0), m_NCksumErrors(0)
   {}

   Stats(const Stats& s) :
      m_NumIos  (s.m_NumIos),   m_Duration(s.m_Duration),
      m_BytesHit(s.m_BytesHit), m_BytesMissed(s.m_BytesMissed), m_BytesBypassed(s.m_BytesBypassed),
      m_BytesWritten(s.m_BytesWritten), m_NCksumErrors(s.m_NCksumErrors)
   {}

   Stats& operator=(const Stats&) = default;

   //----------------------------------------------------------------------

   void AddReadStats(const Stats &s)
   {
      XrdSysMutexHelper _lock(&m_Mutex);

      m_BytesHit      += s.m_BytesHit;
      m_BytesMissed   += s.m_BytesMissed;
      m_BytesBypassed += s.m_BytesBypassed;
   }

   void AddBytesHit(long long bh)
   {
      XrdSysMutexHelper _lock(&m_Mutex);

      m_BytesHit      += bh;
   }

   void AddWriteStats(long long bytes_written, int n_cks_errs)
   {
      XrdSysMutexHelper _lock(&m_Mutex);

      m_BytesWritten += bytes_written;
      m_NCksumErrors += n_cks_errs;
   }

   void IoAttach()
   {
      XrdSysMutexHelper _lock(&m_Mutex);

      ++m_NumIos;
   }

   void IoDetach(int duration)
   {
      XrdSysMutexHelper _lock(&m_Mutex);

      m_Duration += duration;
   }

   Stats Clone()
   {
      XrdSysMutexHelper _lock(&m_Mutex);

      return Stats(*this);
   }

   //----------------------------------------------------------------------

   void DeltaToReference(const Stats& ref)
   {
      // Not locked, only used from Cache / Purge thread.
      m_NumIos        = ref.m_NumIos        - m_NumIos;
      m_Duration      = ref.m_Duration      - m_Duration;
      m_BytesHit      = ref.m_BytesHit      - m_BytesHit;
      m_BytesMissed   = ref.m_BytesMissed   - m_BytesMissed;
      m_BytesBypassed = ref.m_BytesBypassed - m_BytesBypassed;
      m_BytesWritten  = ref.m_BytesWritten  - m_BytesWritten;
      m_NCksumErrors  = ref.m_NCksumErrors  - m_NCksumErrors;
   }

   void AddUp(const Stats& s)
   {
      // Not locked, only used from Cache / Purge thread.
      m_NumIos        += s.m_NumIos;
      m_Duration      += s.m_Duration;
      m_BytesHit      += s.m_BytesHit;
      m_BytesMissed   += s.m_BytesMissed;
      m_BytesBypassed += s.m_BytesBypassed;
      m_BytesWritten  += s.m_BytesWritten;
      m_NCksumErrors  += s.m_NCksumErrors;
   }

   void Reset()
   {
      // Not locked, only used from Cache / Purge thread.
      m_NumIos        = 0;
      m_Duration      = 0;
      m_BytesHit      = 0;
      m_BytesMissed   = 0;
      m_BytesBypassed = 0;
      m_BytesWritten  = 0;
      m_NCksumErrors  = 0;
   }

private:
   XrdSysMutex m_Mutex;
};
}

#endif

