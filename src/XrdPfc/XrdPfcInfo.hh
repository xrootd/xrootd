#ifndef __XRDPFC_INFO_HH__
#define __XRDPFC_INFO_HH__
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

#include <cstdio>
#include <ctime>
#include <assert.h>
#include <vector>

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#include "XrdPfcTypes.hh"

class XrdOssDF;
class XrdCksCalc;
class XrdSysTrace;

namespace XrdPfc
{
class Stats;

//----------------------------------------------------------------------------
//! Status of cached file. Can be read from and written into a binary file.
//----------------------------------------------------------------------------

class Info
{
public:
   struct Status {
      union {
         struct {
            int f_cksum_check : 3;   //!< as in enum CkSumCheck_e

            int _free_bits_ : 29;
         };
         unsigned int _raw_;
      };
      Status() : _raw_(0) {}
   };

   //! Access statistics
   struct AStat
   {
      time_t    AttachTime;       //!< open time
      time_t    DetachTime;       //!< close time
      int       NumIos;           //!< number of IO objects attached during this access
      int       Duration;         //!< total duration of all IOs attached
      int       NumMerged;        //!< number of times the record has been merged
      int       Reserved;         //!< reserved / alignment
      long long BytesHit;         //!< read from cache
      long long BytesMissed;      //!< read from remote and cached
      long long BytesBypassed;    //!< read from remote and dropped

      AStat() :
         AttachTime(0), DetachTime(0), NumIos(0), Duration(0), NumMerged(0), Reserved(0),
         BytesHit(0), BytesMissed(0), BytesBypassed(0)
      {}

      void MergeWith(const AStat &a);
   };

   struct Store
   {
      long long          m_buffer_size;            //!< buffer / block size
      long long          m_file_size;              //!< size of file in bytes
      time_t             m_creationTime;           //!< time the info file was created
      time_t             m_noCkSumTime;            //!< time when first non-cksummed block was detected
      size_t             m_accessCnt;              //!< total access count for the file
      Status             m_status;                 //!< status information
      int                m_astatSize;              //!< size of AStat vector

      Store () :
         m_buffer_size(0), m_file_size(0), m_creationTime(0), m_noCkSumTime(0),
         m_accessCnt(0),   m_astatSize(0)
      {}
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

   void SetBufferSizeFileSizeAndCreationTime(long long bs, long long fs);

   //---------------------------------------------------------------------
   //! \brief Reserve bit vectors for file_size / buffer_size bytes.
   //---------------------------------------------------------------------
   void ResizeBits();

   //---------------------------------------------------------------------
   //! \brief Read content of cinfo file into this object
   //! @param fp    file handle
   //! @param dname directory name for trace output
   //! @param fname optional file name for trace output (can be included in dname)
   //! @return true on success
   //---------------------------------------------------------------------
   bool Read(XrdOssDF* fp, const char *dname, const char *fname = 0);

   //---------------------------------------------------------------------
   //! Write number of blocks and read buffer size
   //! @param fp    file handle
   //! @param dname directory name for trace output
   //! @param fname optional file name for trace output (can be included in dname)
   //! @return true on success
   //---------------------------------------------------------------------
   bool Write(XrdOssDF* fp, const char *dname, const char *fname = 0);

   //---------------------------------------------------------------------
   //! Compactify access records to the configured maximum.
   //---------------------------------------------------------------------
   void CompactifyAccessRecords();

   //---------------------------------------------------------------------
   //! Reset IO Stats
   //---------------------------------------------------------------------
   void ResetAllAccessStats();

   //---------------------------------------------------------------------
   //! Write open time in the last entry of access statistics
   //---------------------------------------------------------------------
   void WriteIOStatAttach();

   //---------------------------------------------------------------------
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
   int CountBlocksNotWrittenInRng(int firstIdx, int lastIdx) const;

   //---------------------------------------------------------------------
   //! Get size of download-state bit-vector in bytes.
   //---------------------------------------------------------------------
   int GetBitvecSizeInBytes() const;

   //---------------------------------------------------------------------
   //! Get number of blocks represented in download-state bit-vector.
   //---------------------------------------------------------------------
   int GetNBlocks() const;

   //---------------------------------------------------------------------
   //! Get file size
   //---------------------------------------------------------------------
   long long GetFileSize() const;

   //---------------------------------------------------------------------
   //! Get latest detach time
   //---------------------------------------------------------------------
   bool GetLatestDetachTime(time_t& t) const;

   //---------------------------------------------------------------------
   //! Get latest access stats
   //---------------------------------------------------------------------
   const AStat* GetLastAccessStats() const;

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
   size_t GetAccessCnt() const { return m_store.m_accessCnt; }

   //---------------------------------------------------------------------
   //! Get version
   //---------------------------------------------------------------------
   int GetVersion() { return m_version; }

   //---------------------------------------------------------------------
   //! Get stored data
   //---------------------------------------------------------------------
   const Store&              RefStoredData() const { return m_store;  }
   const std::vector<AStat>& RefAStats()     const { return m_astats; }

   //---------------------------------------------------------------------
   //! Get file size
   //---------------------------------------------------------------------
   time_t GetCreationTime() const { return m_store.m_creationTime; }

   //---------------------------------------------------------------------
   //! Get cksum, MD5 is for backward compatibility with V2 and V3.
   //---------------------------------------------------------------------
   uint32_t CalcCksumStore();
   uint32_t CalcCksumSyncedAndAStats();
   void     CalcCksumMd5(unsigned char* buff, char* digest);

   CkSumCheck_e GetCkSumState()  const { return (CkSumCheck_e) m_store.m_status.f_cksum_check; }
   const char*  GetCkSumStateAsText() const;

   bool IsCkSumCache() const { return  m_store.m_status.f_cksum_check & CSChk_Cache; }
   bool IsCkSumNet()   const { return  m_store.m_status.f_cksum_check & CSChk_Net;   }
   bool IsCkSumAny()   const { return  m_store.m_status.f_cksum_check & CSChk_Both;  }
   bool IsCkSumBoth()  const { return (m_store.m_status.f_cksum_check & CSChk_Both) == CSChk_Both; }

   void SetCkSumState(CkSumCheck_e css)           { m_store.m_status.f_cksum_check  = css; }
   void DowngradeCkSumState(CkSumCheck_e css_ref) { m_store.m_status.f_cksum_check &= css_ref; }
   void ResetCkSumCache();
   void ResetCkSumNet();

   bool   HasNoCkSumTime() const { return m_store.m_noCkSumTime != 0; }
   time_t GetNoCkSumTime() const { return m_store.m_noCkSumTime; }
   time_t GetNoCkSumTimeForUVKeep() const { return m_store.m_noCkSumTime ? m_store.m_noCkSumTime : m_store.m_creationTime; }
   void   ResetNoCkSumTime() { m_store.m_noCkSumTime = 0; }

#ifdef XRDPFC_CKSUM_TEST
   static void TestCksumStuff();
#endif

   static const char*   m_traceID;          // has to be m_ (convention in TRACE macros)
   static const char*   s_infoExtension;
   static const size_t  s_infoExtensionLen;
   static       size_t  s_maxNumAccess;     // can be set from configuration
   static const int     s_defaultVersion;

   XrdSysTrace* GetTrace() const {return m_trace; }

protected:
   XrdSysTrace*   m_trace;

   Store          m_store;
   unsigned char *m_buff_synced;             //!< disk written state vector
   unsigned char *m_buff_written;            //!< download state vector
   unsigned char *m_buff_prefetch;           //!< prefetch statistics
   std::vector<AStat>  m_astats;             //!< access records

   int  m_version;
   int  m_bitvecSizeInBits;                  //!< cached
   int  m_missingBlocks;                     //!< cached, updated in SetBitWritten()
   bool m_complete;                          //!< cached; if false, set to true when missingBlocks hit zero
   bool m_hasPrefetchBuffer;                 //!< constains current prefetch score

private:
   inline unsigned char cfiBIT(int n) const { return 1 << n; }

   // Reading functions for older cinfo file formats
   bool ReadV2(XrdOssDF* fp, off_t off, const char *dname, const char *fname);
   bool ReadV3(XrdOssDF* fp, off_t off, const char *dname, const char *fname);

   XrdCksCalc*   m_cksCalcMd5;
};

//------------------------------------------------------------------------------

inline bool Info::TestBitWritten(int i) const
{
   const int cn = i/8;
   assert(cn < GetBitvecSizeInBytes());

   const int off = i - cn*8;
   return (m_buff_written[cn] & cfiBIT(off)) != 0;
}

inline void Info::SetBitWritten(int i)
{
   const int cn = i/8;
   assert(cn < GetBitvecSizeInBytes());

   const int off = i - cn*8;

   m_buff_written[cn] |= cfiBIT(off);

   if (--m_missingBlocks == 0)
      m_complete = true;
}

inline void Info::SetBitPrefetch(int i)
{
   if (!m_buff_prefetch) return;
      
   const int cn = i/8;
   assert(cn < GetBitvecSizeInBytes());

   const int off = i - cn*8;
   m_buff_prefetch[cn] |= cfiBIT(off);
}

inline bool Info::TestBitPrefetch(int i) const
{
   if (!m_buff_prefetch) return false;

   const int cn = i/8;
   assert(cn < GetBitvecSizeInBytes());

   const int off = i - cn*8;
   return (m_buff_prefetch[cn] & cfiBIT(off)) != 0;
}

inline void Info::SetBitSynced(int i)
{
   const int cn = i/8;
   assert(cn < GetBitvecSizeInBytes());

   const int off = i - cn*8;
   m_buff_synced[cn] |= cfiBIT(off);
}

//------------------------------------------------------------------------------

inline int Info::GetNDownloadedBlocks() const
{
   int cntd = 0;
   for (int i = 0; i < m_bitvecSizeInBits; ++i)
      if (TestBitWritten(i)) cntd++;

   return cntd;
}

inline long long Info::GetNDownloadedBytes() const
{
   return m_store.m_buffer_size * GetNDownloadedBlocks();
}

inline int Info::GetLastDownloadedBlock() const
{
   for (int i = m_bitvecSizeInBits - 1; i >= 0; --i)
      if (TestBitWritten(i)) return i;

   return -1;
}

inline long long Info::GetExpectedDataFileSize() const
{
   int last_block = GetLastDownloadedBlock();
   if (last_block == m_bitvecSizeInBits - 1)
      return m_store.m_file_size;
   else
      return (last_block + 1) * m_store.m_buffer_size;
}

inline int Info::GetBitvecSizeInBytes() const
{
   if (m_bitvecSizeInBits)
      return ((m_bitvecSizeInBits - 1)/8 + 1);
   else
      return 0;
}

inline int Info::GetNBlocks() const
{
   return m_bitvecSizeInBits;
}

inline long long Info::GetFileSize() const
{
   return m_store.m_file_size;
}

inline bool Info::IsComplete() const
{
   return m_complete;
}

inline int Info::CountBlocksNotWrittenInRng(int firstIdx, int lastIdx) const
{
   // TODO rewrite to use full byte comparisons outside of edges ?
   // Also, it seems to be always called with firstIdx = 0, lastIdx = m_bitvecSizeInBits.
   int cnt = 0;
   for (int i = firstIdx; i < lastIdx; ++i)
      if (! TestBitWritten(i)) ++cnt;

   return cnt;
}

inline void Info::UpdateDownloadCompleteStatus()
{
   m_missingBlocks = CountBlocksNotWrittenInRng(0, m_bitvecSizeInBits);
   m_complete      = (m_missingBlocks == 0);
}

inline long long Info::GetBufferSize() const
{
   return m_store.m_buffer_size;
}

}
#endif
