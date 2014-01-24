#ifndef __XRDFILECACHE_IO_FILE_BLOCK_HH__
#define __XRDFILECACHE_IO_FILE_BLOCK_HH__
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
#include <map>
#include <string>

#include <XrdOuc/XrdOucCache.hh>
#include "XrdSys/XrdSysPthread.hh"

#include "XrdFileCache.hh"
#include "XrdFileCachePrefetch.hh"

class XrdSysError;
class XrdOssDF;

namespace XrdFileCache
{
   //----------------------------------------------------------------------------
   //! Downloads original data into multiple files. Handles read requests.
   //----------------------------------------------------------------------------
   class IOFileBlock : public IO
   {
      public:
         //------------------------------------------------------------------------
         //! Constructor
         //------------------------------------------------------------------------      
         IOFileBlock(XrdOucCacheIO &io, XrdOucCacheStats &stats, Cache &cache);

         //------------------------------------------------------------------------
         //! Destructor
         //------------------------------------------------------------------------ 
         ~IOFileBlock() {}

         //---------------------------------------------------------------------
         //!\brief Detach itself from Cache. Note this will delete this object.
         //!
         //! @return original source \ref XrdPosixFile
         //---------------------------------------------------------------------
         virtual XrdOucCacheIO *Detach();

         //---------------------------------------------------------------------
         //!\brief ReadV pass vector reads to corresponding Prefetch blocks.
         //!
         //! @param readV
         //! @param n number of XrdOucIOVecs
         //!
         //! @return total bytes read
         //---------------------------------------------------------------------
         virtual int Read (char  *Buffer, long long Offset, int Length);

      private:
         struct FileBlock
         {
            FileBlock(off_t off, XrdOucCacheIO*  io) :  m_prefetch(0), m_offset0(off) {}
            Prefetch* m_prefetch;
            long long m_offset0;
         };

         long long                  m_blockSize; //!< size of file-block
         std::map<int, FileBlock*>  m_blocks;    //!< map of created blocks
         XrdSysMutex                m_mutex;     //!< map mutex

         FileBlock*  newBlockPrefetcher(long long off, int blocksize, XrdOucCacheIO*  io);
   };

}
#endif
