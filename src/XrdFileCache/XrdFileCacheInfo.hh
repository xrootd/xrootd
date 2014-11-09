#ifndef __XRDFILECACHE_INFO_HH__
#define __XRDFILECACHE_INFO_HH__
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

#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

class XrdOssDF;

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
      private:
         static unsigned char cfiBIT(int n) { return 1 << n; }

      public:
         //------------------------------------------------------------------------
         //! Constructor.
         //------------------------------------------------------------------------
         Info();

         //------------------------------------------------------------------------
         //! Destructor.
         //------------------------------------------------------------------------
         ~Info();

         //---------------------------------------------------------------------
         //! \brief Mark block as downloaded
         //!
         //! @param i block index
         //---------------------------------------------------------------------
         void SetBit(int i);

         //---------------------------------------------------------------------
         //! \brief Reserve buffer for fileSize/bufferSize bytes
         //!
         //! @param n number of file blocks
         //---------------------------------------------------------------------
         void ResizeBits(int n);

         //---------------------------------------------------------------------
         //! \brief Rea load content from cinfo file into this object
         //!
         //! @param fp file handle
         //!
         //! @return number of bytes read
         //---------------------------------------------------------------------
         int Read(XrdOssDF* fp);

         //---------------------------------------------------------------------
         //! Write number of blocks and prefetch buffer size
         //---------------------------------------------------------------------
         void  WriteHeader(XrdOssDF* fp);

         //---------------------------------------------------------------------
         //! Append access time, and cache statistics
         //---------------------------------------------------------------------
         void AppendIOStat(const Stats* stat, XrdOssDF* fp);

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

         //----------------------------------------------------------------------
         //! Get header size.
         //----------------------------------------------------------------------
         int GetHeaderSize() const;

         //---------------------------------------------------------------------
         //! Get latest detach time
         //---------------------------------------------------------------------
         bool GetLatestDetachTime(time_t& t, XrdOssDF* fp) const;

         //---------------------------------------------------------------------
         //! Get prefetch buffer size
         //---------------------------------------------------------------------
         long long GetBufferSize() const;

         //---------------------------------------------------------------------
         //! Test if block at the given index is downlaoded
         //---------------------------------------------------------------------
         bool TestBit(int i) const;

         //---------------------------------------------------------------------
         //! Get complete status
         //---------------------------------------------------------------------
         bool IsComplete() const;

         //---------------------------------------------------------------------
         //! Update complete status
         //---------------------------------------------------------------------
         void CheckComplete();

         const static char* m_infoExtension;

      private:

         XrdCl::Log* clLog() const { return XrdCl::DefaultEnv::GetLog(); }

         //---------------------------------------------------------------------
         //! Cache statistics and time of access.
         //---------------------------------------------------------------------
         struct AStat
         {
            time_t    DetachTime;
            long long BytesDisk;
            long long BytesRam;
            long long BytesMissed;
         };

         int            m_version;    //!< info version
         long long      m_bufferSize; //!< prefetch buffer size
         int            m_sizeInBits; //!< number of file blocks
         unsigned char *m_buff;       //!< download state vector
         int            m_accessCnt;  //!< number of written AStat structs
         bool           m_complete;   //!< cached
   };

   inline bool Info::TestBit(int i) const
   {
      int cn = i/8;
      assert(cn < GetSizeInBytes());

      int off = i - cn*8;
      return (m_buff[cn] & cfiBIT(off)) == cfiBIT(off);
   }

   inline int Info::GetSizeInBytes() const
   {
      return ((m_sizeInBits -1)/8 + 1);
   }

   inline int Info::GetSizeInBits() const
   {
      return m_sizeInBits;
   }

   inline bool Info::IsComplete() const
   {
      return m_complete;
   }

   inline bool Info::IsAnythingEmptyInRng(int firstIdx, int lastIdx) const
   {
      for (int i = firstIdx; i <= lastIdx; ++i)
         if (!TestBit(i)) return true;

      return false;
   }

   inline void Info::CheckComplete()
   {
      m_complete = !IsAnythingEmptyInRng(0, m_sizeInBits-1);
   }

   inline void Info::SetBit(int i)
   {
      int cn = i/8;
      assert(cn < GetSizeInBytes());

      int off = i - cn*8;
      m_buff[cn] |= cfiBIT(off);
   }

   inline long long Info::GetBufferSize() const
   {
      return m_bufferSize;
   }
}
#endif
