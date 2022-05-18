#ifndef __XRDPFC_IO_FILE_BLOCK_HH__
#define __XRDPFC_IO_FILE_BLOCK_HH__
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

#include "XrdOuc/XrdOucCache.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "XrdPfcIO.hh"

class XrdSysError;
class XrdOssDF;

namespace XrdPfc
{
//----------------------------------------------------------------------------
//! \brief Downloads original file into multiple files, chunked into
//! blocks. Only blocks that are asked for are downloaded.
//! Handles read requests as they come along.
//----------------------------------------------------------------------------
class IOFileBlock : public IO
{
public:
   IOFileBlock(XrdOucCacheIO *io, Cache &cache);

   ~IOFileBlock();

   //! \brief Abstract virtual method of XrdPfc::IO
   //! Called to check if destruction needs to be done in a separate task.
   bool ioActive() override;

   //! \brief Abstract virtual method of XrdPfc::IO
   //! Called to destruct the IO object after it is no longer used.
   void DetachFinalize() override;

   //---------------------------------------------------------------------
   //! Pass Read request to the corresponding File object.
   //---------------------------------------------------------------------
   using XrdOucCacheIO::Read;

   int Read(char *Buffer, long long Offset, int Length) override;
   
   int  Fstat(struct stat &sbuff) override;

   long long FSize() override;

   void Update(XrdOucCacheIO &iocp) override;

private:
   long long                  m_blocksize;       //!< size of file-block
   std::map<int, File*>       m_blocks;          //!< map of created blocks
   XrdSysMutex                m_mutex;           //!< map mutex
   struct stat               *m_localStat;
   Info                       m_info;
   XrdOssDF*                  m_info_file;

   void  GetBlockSizeFromPath();
   int   initLocalStat();
   File* newBlockFile(long long off, int blocksize);
   void  CloseInfoFile();
};
}

#endif
