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

namespace XrdPfc
{

//----------------------------------------------------------------------------
//! Statistics of cache utilisation by a File object.
//  Used both as aggregation of usage by a single file as well as for
//  collecting per-directory statistics on time-interval basis. In this second
//  case they are used as "deltas" ... differences in respect to a previous
//  reference value.
//  For running averages / deltas, one might need a version with doubles, so
//  it might make sense to template this. And add some timestamp.
//----------------------------------------------------------------------------
class Stats
{
public:
   int       m_NumIos = 0;          //!< number of IO objects attached during this access
   int       m_Duration = 0;        //!< total duration of all IOs attached
   long long m_BytesHit = 0;        //!< number of bytes served from disk
   long long m_BytesMissed = 0;     //!< number of bytes served from remote and cached
   long long m_BytesBypassed = 0;   //!< number of bytes served directly through XrdCl
   long long m_BytesWritten = 0;    //!< number of bytes written to disk
   long long m_StBlocksAdded = 0;   //!< number of 512-byte blocks the file has grown by
   int       m_NCksumErrors = 0;    //!< number of checksum errors while getting data from remote

   //----------------------------------------------------------------------

   Stats() = default;

   Stats(const Stats& s) = default;

   Stats& operator=(const Stats&) = default;

   Stats(const Stats& a, const Stats& b) :
      m_NumIos        (a.m_NumIos       + b.m_NumIos),
      m_Duration      (a.m_Duration      + b.m_Duration),
      m_BytesHit      (a.m_BytesHit      + b.m_BytesHit),
      m_BytesMissed   (a.m_BytesMissed   + b.m_BytesMissed),
      m_BytesBypassed (a.m_BytesBypassed + b.m_BytesBypassed),
      m_BytesWritten  (a.m_BytesWritten  + b.m_BytesWritten),
      m_StBlocksAdded (a.m_StBlocksAdded + b.m_StBlocksAdded),
      m_NCksumErrors  (a.m_NCksumErrors  + b.m_NCksumErrors)
   {}

   //----------------------------------------------------------------------

   void AddReadStats(const Stats &s)
   {
      m_BytesHit      += s.m_BytesHit;
      m_BytesMissed   += s.m_BytesMissed;
      m_BytesBypassed += s.m_BytesBypassed;
   }

   void AddBytesHit(long long bh)
   {
      m_BytesHit      += bh;
   }

   void AddWriteStats(long long bytes_written, int n_cks_errs)
   {
      m_BytesWritten += bytes_written;
      m_NCksumErrors += n_cks_errs;
   }

   void IoAttach()
   {
      ++m_NumIos;
   }

   void IoDetach(int duration)
   {
      m_Duration += duration;
   }

   //----------------------------------------------------------------------

   long long BytesRead() const
   {
      return m_BytesHit + m_BytesMissed + m_BytesBypassed;
   }

   long long BytesReadAndWritten() const
   {
      return BytesRead() + m_BytesWritten;
   }

   void DeltaToReference(const Stats& ref)
   {
      m_NumIos        = ref.m_NumIos        - m_NumIos;
      m_Duration      = ref.m_Duration      - m_Duration;
      m_BytesHit      = ref.m_BytesHit      - m_BytesHit;
      m_BytesMissed   = ref.m_BytesMissed   - m_BytesMissed;
      m_BytesBypassed = ref.m_BytesBypassed - m_BytesBypassed;
      m_BytesWritten  = ref.m_BytesWritten  - m_BytesWritten;
      m_StBlocksAdded = ref.m_StBlocksAdded - m_StBlocksAdded;
      m_NCksumErrors  = ref.m_NCksumErrors  - m_NCksumErrors;
   }

   void AddUp(const Stats& s)
   {
      m_NumIos        += s.m_NumIos;
      m_Duration      += s.m_Duration;
      m_BytesHit      += s.m_BytesHit;
      m_BytesMissed   += s.m_BytesMissed;
      m_BytesBypassed += s.m_BytesBypassed;
      m_BytesWritten  += s.m_BytesWritten;
      m_StBlocksAdded += s.m_StBlocksAdded;
      m_NCksumErrors  += s.m_NCksumErrors;
   }

   void Reset()
   {
      m_NumIos        = 0;
      m_Duration      = 0;
      m_BytesHit      = 0;
      m_BytesMissed   = 0;
      m_BytesBypassed = 0;
      m_BytesWritten  = 0;
      m_StBlocksAdded = 0;
      m_NCksumErrors  = 0;
   }
};

//==============================================================================

class DirStats : public Stats
{
public:
   long long m_StBlocksRemoved = 0; // number of 512-byte blocks removed from the directory
   int       m_NFilesOpened = 0;
   int       m_NFilesClosed = 0;
   int       m_NFilesCreated = 0;
   int       m_NFilesRemoved = 0; // purged or otherwise (error, direct requests)
   int       m_NDirectoriesCreated = 0;
   int       m_NDirectoriesRemoved = 0;

   //----------------------------------------------------------------------

   DirStats() = default;

   DirStats(const DirStats& s) = default;

   DirStats& operator=(const DirStats&) = default;

   DirStats(const DirStats& a, const DirStats& b) :
      Stats(a, b),
      m_StBlocksRemoved     (a.m_StBlocksRemoved     + b.m_StBlocksRemoved),
      m_NFilesOpened        (a.m_NFilesOpened        + b.m_NFilesOpened),
      m_NFilesClosed        (a.m_NFilesClosed        + b.m_NFilesClosed),
      m_NFilesCreated       (a.m_NFilesCreated       + b.m_NFilesCreated),
      m_NFilesRemoved       (a.m_NFilesRemoved       + b.m_NFilesRemoved),
      m_NDirectoriesCreated (a.m_NDirectoriesCreated + b.m_NDirectoriesCreated),
      m_NDirectoriesRemoved (a.m_NDirectoriesRemoved + b.m_NDirectoriesRemoved)
   {}

   //----------------------------------------------------------------------

   using Stats::DeltaToReference; // activate overload based on arg
   void DeltaToReference(const DirStats& ref)
   {
      Stats::DeltaToReference(ref);
      m_StBlocksRemoved     = ref.m_StBlocksRemoved     - m_StBlocksRemoved;
      m_NFilesOpened        = ref.m_NFilesOpened        - m_NFilesOpened;
      m_NFilesClosed        = ref.m_NFilesClosed        - m_NFilesClosed;
      m_NFilesCreated       = ref.m_NFilesCreated       - m_NFilesCreated;
      m_NFilesRemoved       = ref.m_NFilesRemoved       - m_NFilesRemoved;
      m_NDirectoriesCreated = ref.m_NDirectoriesCreated - m_NDirectoriesCreated;
      m_NDirectoriesRemoved = ref.m_NDirectoriesRemoved - m_NDirectoriesRemoved;
   }

   using Stats::AddUp; // activate overload based on arg
   void AddUp(const DirStats& s)
   {
      Stats::AddUp(s);
      m_StBlocksRemoved     += s.m_StBlocksRemoved;
      m_NFilesOpened        += s.m_NFilesOpened;
      m_NFilesClosed        += s.m_NFilesClosed;
      m_NFilesCreated       += s.m_NFilesCreated;
      m_NFilesRemoved       += s.m_NFilesRemoved;
      m_NDirectoriesCreated += s.m_NDirectoriesCreated;
      m_NDirectoriesRemoved += s.m_NDirectoriesRemoved;
   }

   using Stats::Reset; // activate overload based on arg
   void Reset()
   {
      Stats::Reset();
      m_StBlocksRemoved     = 0;
      m_NFilesOpened        = 0;
      m_NFilesClosed        = 0;
      m_NFilesCreated       = 0;
      m_NFilesRemoved       = 0;
      m_NDirectoriesCreated = 0;
      m_NDirectoriesRemoved = 0;
   }
};

}

#endif
