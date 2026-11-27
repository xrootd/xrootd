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
//! Statistics of cache utilisation by a File object: its access record, and
//! the delta it reports to the ResourceMonitor.
//----------------------------------------------------------------------------
class FileStats
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

   void AddReadStats(const FileStats &s)
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

   void AddUp(const FileStats& s)
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
      *this = FileStats();
   }
};

//----------------------------------------------------------------------------
//! Statistics of a directory: the FileStats of its files, summed, and the
//! changes to its files and subdirectories. 64-bit throughout, as they are
//! summed over subtrees and reporting intervals.
//----------------------------------------------------------------------------
class DirStats
{
public:
   long long m_NumIos = 0;
   long long m_Duration = 0;
   long long m_BytesHit = 0;
   long long m_BytesMissed = 0;
   long long m_BytesBypassed = 0;
   long long m_BytesWritten = 0;
   long long m_StBlocksAdded = 0;
   long long m_NCksumErrors = 0;
   long long m_StBlocksRemoved = 0;     //!< number of 512-byte blocks removed from the directory
   long long m_NFilesOpened = 0;
   long long m_NFilesClosed = 0;
   long long m_NFilesCreated = 0;
   long long m_NFilesRemoved = 0;       //!< purged or otherwise (error, direct requests)
   long long m_NDirectoriesCreated = 0;
   long long m_NDirectoriesRemoved = 0;

   //----------------------------------------------------------------------

   void AddUp(const FileStats& s)
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

   void AddUp(const DirStats& s)
   {
      m_NumIos              += s.m_NumIos;
      m_Duration            += s.m_Duration;
      m_BytesHit            += s.m_BytesHit;
      m_BytesMissed         += s.m_BytesMissed;
      m_BytesBypassed       += s.m_BytesBypassed;
      m_BytesWritten        += s.m_BytesWritten;
      m_StBlocksAdded       += s.m_StBlocksAdded;
      m_NCksumErrors        += s.m_NCksumErrors;
      m_StBlocksRemoved     += s.m_StBlocksRemoved;
      m_NFilesOpened        += s.m_NFilesOpened;
      m_NFilesClosed        += s.m_NFilesClosed;
      m_NFilesCreated       += s.m_NFilesCreated;
      m_NFilesRemoved       += s.m_NFilesRemoved;
      m_NDirectoriesCreated += s.m_NDirectoriesCreated;
      m_NDirectoriesRemoved += s.m_NDirectoriesRemoved;
   }

   void Reset()
   {
      *this = DirStats();
   }
};

}

#endif
