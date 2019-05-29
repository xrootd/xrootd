#ifndef __XRDFILECACHE_INFO_HH__
#define __XRDFILECACHE_INFO_HH__
//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz  Tadel, Brian Bockelman
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

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <vector>

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

class XrdOssDF;
class XrdCksCalc;
class XrdSysTrace;


namespace XrdCl
{
class Log;
}

namespace XrdFileCache
{
class Stats;

//----------------------------------------------------------------------------
//! Status of cached file. Can be read from and written into a binary file.
//----------------------------------------------------------------------------

class Info
{
public:
   // !Access statistics
   struct AStat
   {
      time_t    AttachTime;       //! open time
      time_t    DetachTime;       //! close time
      long long BytesDisk;        //! read from disk
      long long BytesRam;         //! read from ram
      long long BytesMissed;      //! read remote client

      AStat() : AttachTime(0), DetachTime(0), BytesDisk(0), BytesRam(0), BytesMissed(0) {}
   };

   struct Store {
      int                m_version;                //!< info version
      long long          m_bufferSize;             //!< prefetch buffer size
      long long          m_fileSize;               //!< number of file blocks
      unsigned char     *m_buff_synced;            //!< disk written state vector
      char               m_cksum[16];              //!< cksum of downloaded information
      time_t             m_creationTime;           //!< time the info file was created
      size_t             m_accessCnt;              //!< number of written AStat structs
      std::vector<AStat> m_astats;                 //!< number of last m_maxAcessCnts

      Store () : m_version(1), m_bufferSize(-1), m_fileSize(0), m_buff_synced(0),m_creationTime(0), m_accessCnt(0) {}
   };


   //------------------------------------------------------------------------
   //! Constructor.
   //------------------------------------------------------------------------
   Info(XrdSysTrace* trace, bool prefetchBuffer = false);

   //------------------------------------------------------------------------
   //! Destructor.
   //------------------------------------------------------------------------
   ~Info();

   //---------------------------------------------------------------------
   //! Mark block as written to disk
   //---------------------------------------------------------------------
   void SetBitWritten(int i);

   //---------------------------------------------------------------------
   //! Test if block at the given index is written to disk
   //---------------------------------------------------------------------
   bool TestBitWritten(int i) const;

   //---------------------------------------------------------------------
   //! Test if block at the given index has been prefetched
   //---------------------------------------------------------------------
   bool TestBitPrefetch(int i) const;

   //---------------------------------------------------------------------
   //! Mark block as obtained through prefetch
   //---------------------------------------------------------------------
   void SetBitPrefetch(int i);

   //---------------------------------------------------------------------
   //! Mark block as synced to disk
   //---------------------------------------------------------------------
   void SetBitSynced(int i);

   //---------------------------------------------------------------------
   //! Mark all blocks as synced to disk
   //---------------------------------------------------------------------
   void SetAllBitsSynced();

   void SetBufferSize(long long);
   
   void SetFileSize(long long);

   //---------------------------------------------------------------------
   //! \brief Reserve buffer for fileSize/bufferSize bytes
   //!
   //! @param n number of file blocks
   //---------------------------------------------------------------------
   void ResizeBits(int n);

   //---------------------------------------------------------------------
   //! \brief Rea load content from cinfo file into this object
   //!
   //! @param fp    file handle
   //! @param fname optional file name for trace output
   //!
   //! @return true on success
   //---------------------------------------------------------------------
   bool Read(XrdOssDF* fp, const std::string &fname = "<unknown>");

   //---------------------------------------------------------------------
   //! Write number of blocks and read buffer size
   //! @return true on success
   //---------------------------------------------------------------------
   bool Write(XrdOssDF* fp, const std::string &fname = "<unknown>");

   //---------------------------------------------------------------------
   //! Disable allocating, writing, and reading of downlaod status
   //---------------------------------------------------------------------
   void DisableDownloadStatus();

   //---------------------------------------------------------------------
   //! Reset IO Stats
   //---------------------------------------------------------------------
   void ResetAllAccessStats();

   //---------------------------------------------------------------------
   //! Write open time in the last entry of access statistics
   //---------------------------------------------------------------------
   void WriteIOStatAttach();

   //! Write bytes missed, hits, and disk
   //---------------------------------------------------------------------
   void WriteIOStat(Stats& s);

  //---------------------------------------------------------------------
   //! Write close time together with bytes missed, hits, and disk
   //---------------------------------------------------------------------
   void WriteIOStatDetach(Stats& s);

   //---------------------------------------------------------------------
   //! Write single open/close time for given bytes read from disk.
   //---------------------------------------------------------------------
   void WriteIOStatSingle(long long bytes_disk);

   //---------------------------------------------------------------------
   //! Write open/close with given time and bytes read from disk.
   //---------------------------------------------------------------------
   void WriteIOStatSingle(long long bytes_disk, time_t att, time_t dtc);

   //---------------------------------------------------------------------
   //! Check download status in given block range
   //---------------------------------------------------------------------
   bool IsAnythingEmptyInRng(int firstIdx, int lastIdx) const;

   //---------------------------------------------------------------------
   //! Get size of download-state bit-vector in bytes.
   //---------------------------------------------------------------------
   int GetSizeInBytes() const;

   //---------------------------------------------------------------------
   //! Get number of blocks represented in download-state bit-vector.
   //---------------------------------------------------------------------
   int GetSizeInBits() const;

   //---------------------------------------------------------------------
   //! Get file size
   //---------------------------------------------------------------------
   long long GetFileSize() const;

   //---------------------------------------------------------------------
   //! Get latest detach time
   //---------------------------------------------------------------------
   bool GetLatestDetachTime(time_t& t) const;

   //---------------------------------------------------------------------
   //! Get prefetch buffer size
   //---------------------------------------------------------------------
   long long GetBufferSize() const;

   //---------------------------------------------------------------------
   //! Get complete status
   //---------------------------------------------------------------------
   bool IsComplete() const;

   //---------------------------------------------------------------------
   //! Get number of downloaded blocks
   //---------------------------------------------------------------------
   int GetNDownloadedBlocks() const;

   //---------------------------------------------------------------------
   //! Get number of downloaded bytes
   //---------------------------------------------------------------------
   long long GetNDownloadedBytes() const;

   //---------------------------------------------------------------------
   //! Get number of the last downloaded block
   //---------------------------------------------------------------------
   int GetLastDownloadedBlock() const;

   //---------------------------------------------------------------------
   //! Get expected data file size
   //---------------------------------------------------------------------
   long long GetExpectedDataFileSize() const;

  //---------------------------------------------------------------------
   //! Update complete status
   //---------------------------------------------------------------------
   void UpdateDownloadCompleteStatus();

   //---------------------------------------------------------------------
   //! Get number of accesses
   //---------------------------------------------------------------------
   size_t GetAccessCnt() { return m_store.m_accessCnt; }

   //---------------------------------------------------------------------
   //! Get version
   //---------------------------------------------------------------------
   int GetVersion() { return m_store.m_version; }

   //---------------------------------------------------------------------
   //! Get stored data
   //---------------------------------------------------------------------
   const Store& RefStoredData() const { return m_store; }

   //---------------------------------------------------------------------
   //! Get md5 cksum
   //---------------------------------------------------------------------
   void GetCksum( unsigned char* buff, char* digest);

   const static char*   m_infoExtension;
   const static char*   m_traceID;
   const static int     m_defaultVersion;
   const static size_t  m_maxNumAccess;

   XrdSysTrace* GetTrace() const {return m_trace; }

   static size_t GetMaxNumAccess() { return m_maxNumAccess; }

protected:
   XrdSysTrace*   m_trace;

   Store          m_store;
   bool           m_hasPrefetchBuffer;       //!< constains current prefetch score
   unsigned char *m_buff_written;            //!< download state vector
   unsigned char *m_buff_prefetch;           //!< prefetch statistics

   int  m_sizeInBits;                        //!< cached
   bool m_complete;                          //!< cached

private:
   inline unsigned char cfiBIT(int n) const { return 1 << n; }

   // split reading for V1
   bool ReadV1(XrdOssDF* fp, const std::string &fname);
   XrdCksCalc*   m_cksCalc;
};

//------------------------------------------------------------------------------

inline bool Info::TestBitWritten(int i) const
{
   const int cn = i/8;
   assert(cn < GetSizeInBytes());

   const int off = i - cn*8;
   return (m_buff_written[cn] & cfiBIT(off)) != 0;
}

inline void Info::SetBitWritten(int i)
{
   const int cn = i/8;
   assert(cn < GetSizeInBytes());

   const int off = i - cn*8;
   m_buff_written[cn] |= cfiBIT(off);
}

inline void Info::SetBitPrefetch(int i)
{
   if (!m_buff_prefetch) return;
      
   const int cn = i/8;
   assert(cn < GetSizeInBytes());

   const int off = i - cn*8;
   m_buff_prefetch[cn] |= cfiBIT(off);
}

inline bool Info::TestBitPrefetch(int i) const
{
   if (!m_buff_prefetch) return false;

   const int cn = i/8;
   assert(cn < GetSizeInBytes());

   const int off = i - cn*8;
   return (m_buff_prefetch[cn] & cfiBIT(off)) != 0;
}

inline void Info::SetBitSynced(int i)
{
   const int cn = i/8;
   assert(cn < GetSizeInBytes());

   const int off = i - cn*8;
   m_store.m_buff_synced[cn] |= cfiBIT(off);
}

//------------------------------------------------------------------------------

inline int Info::GetNDownloadedBlocks() const
{
   int cntd = 0;
   for (int i = 0; i < m_sizeInBits; ++i)
      if (TestBitWritten(i)) cntd++;

   return cntd;
}

inline long long Info::GetNDownloadedBytes() const
{
   return m_store.m_bufferSize * GetNDownloadedBlocks();
}

inline int Info::GetLastDownloadedBlock() const
{
   for (int i = m_sizeInBits - 1; i >= 0; --i)
      if (TestBitWritten(i)) return i;

   return -1;
}

inline long long Info::GetExpectedDataFileSize() const
{
   int last_block = GetLastDownloadedBlock();
   if (last_block == m_sizeInBits - 1)
      return m_store.m_fileSize;
   else
      return (last_block + 1) * m_store.m_bufferSize;
}

inline int Info::GetSizeInBytes() const
{
   if (m_sizeInBits)
      return ((m_sizeInBits - 1)/8 + 1);
   else
      return 0;
}

inline int Info::GetSizeInBits() const
{
   return m_sizeInBits;
}

inline long long Info::GetFileSize() const
{
   return m_store.m_fileSize;
}

inline bool Info::IsComplete() const
{
   return m_complete;
}

inline bool Info::IsAnythingEmptyInRng(int firstIdx, int lastIdx) const
{
   // TODO rewrite to use full byte comparisons outside of edges ?
   // Also, it is always called with fisrtsdx = 0, lastIdx = m_sizeInBits.
   for (int i = firstIdx; i < lastIdx; ++i)
      if (! TestBitWritten(i)) return true;

   return false;
}

inline void Info::UpdateDownloadCompleteStatus()
{
   m_complete = ! IsAnythingEmptyInRng(0, m_sizeInBits);
}

inline long long Info::GetBufferSize() const
{
   return m_store.m_bufferSize;
}

}
#endif
