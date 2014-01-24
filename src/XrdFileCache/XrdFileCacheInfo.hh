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

#include <XrdSys/XrdSysPthread.hh>
class XrdOssDF;

#define cfiBIT(n)       (1ULL << (n))

namespace XrdFileCache
{
   class Stats;

   //----------------------------------------------------------------------------
   //! Status of cached file. Has utility to read and dump to info file.
   //----------------------------------------------------------------------------  
   class Info
   {
      public:
         //------------------------------------------------------------------------
         //! Constructor
         //------------------------------------------------------------------------        
         Info();

         //------------------------------------------------------------------------
         //! Destructor
         //------------------------------------------------------------------------    
         ~Info();

         //---------------------------------------------------------------------
         //! SetBit marks block in the given index as downloaded
         //!
         //! @param i
         //---------------------------------------------------------------------
         void setBit(int i);

         //---------------------------------------------------------------------
         //! Reserve buffer for fileSize/bufferSize blocks
         //!
         //! @param n number of file blocks
         //---------------------------------------------------------------------
         void resizeBits(int n);

         //---------------------------------------------------------------------
         //! \brief Read load content from *cinfo file into this object
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
         //! Check download status in the block range
         //---------------------------------------------------------------------
         bool isAnythingEmptyInRng(int firstIdx, int lastIdx) const;

         //---------------------------------------------------------------------
         //! Get compressed size
         //---------------------------------------------------------------------
         int getSizeInBytes() const;

         //---------------------------------------------------------------------
         //! Get number of blocks
         //---------------------------------------------------------------------
         int getSizeInBits() const;

         //----------------------------------------------------------------------
         //! Get header size
         //----------------------------------------------------------------------
         int getHeaderSize() const;

         //---------------------------------------------------------------------
         //! Get latest detach time
         //---------------------------------------------------------------------
         bool getLatestDetachTime(time_t& t, XrdOssDF* fp) const;

         //---------------------------------------------------------------------
         //! getBufferSize get prefetch buffer size
         //---------------------------------------------------------------------
         long long getBufferSize() const;

         //---------------------------------------------------------------------
         // Test if block at the given index is downlaoded
         //---------------------------------------------------------------------
         bool testBit(int i) const;

         //---------------------------------------------------------------------
         //! isComplete checks file is completely downloaded 
         //---------------------------------------------------------------------
         bool isComplete() const;

         //---------------------------------------------------------------------
         //! checkComplete Refresh complete status
         //---------------------------------------------------------------------
         void checkComplete();

         //---------------------------------------------------------------------
         //! print printout content. Need only for debugging
         //---------------------------------------------------------------------
         void print() const;

         const static char* m_infoExtension;

      private:
         //---------------------------------------------------------------------
         //! Written into *cinfo file each time cached data is accessed.
         //---------------------------------------------------------------------
         struct AStat
         {
            time_t    DetachTime;
            long long BytesRead;
            int       Hits;
            int       Miss;

            void Dump() const {
               printf("AStat value: detach %d, bytesRead = %lld, Hits = %d, Miss = %d \n",
                      (int)DetachTime, BytesRead, Hits, Miss );
            }
         };

         long long   m_bufferSize; //!< prefetch buffer size
         int         m_sizeInBits; //!< number of file blocks
         char*       m_buff;       //!< download state vector
         int         m_accessCnt;  //!< number of written AStat structs
         bool        m_complete;   //!< cached
   };

   inline bool Info::testBit(int i) const
   {
      int cn = i/8;
      assert(cn < getSizeInBytes());

      int off = i - cn*8;
      return (m_buff[cn] & cfiBIT(off)) == cfiBIT(off);
   }

   inline int Info::getSizeInBytes() const
   {
      return ((m_sizeInBits -1)/8 + 1);
   }

   inline int Info::getSizeInBits() const
   {
      return m_sizeInBits;
   }

   inline bool Info::isComplete() const
   {
      return m_complete;
   }

   inline bool Info::isAnythingEmptyInRng(int firstIdx, int lastIdx) const
   {
      for (int i = firstIdx; i <= lastIdx; ++i)
         if(!testBit(i)) return true;

      return false;
   }

   inline void Info::checkComplete()
   {
      m_complete = !isAnythingEmptyInRng(0, m_sizeInBits-1);
   }

   inline void Info::setBit(int i)
   {
      int cn = i/8;
      assert(cn < getSizeInBytes());

      int off = i - cn*8;
      m_buff[cn] |= cfiBIT(off);
   }

   inline long long Info::getBufferSize() const
   {
      return m_bufferSize;
   }
}
#endif
