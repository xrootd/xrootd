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

#include <math.h>
#include <sstream>
#include <stdio.h>
#include <iostream>
#include <assert.h>

#include "XrdFileCacheIOFileBlock.hh"
#include "XrdFileCache.hh"
#include "XrdFileCacheStats.hh"
#include "XrdFileCacheTrace.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSfs/XrdSfsInterface.hh"


using namespace XrdFileCache;

//______________________________________________________________________________
IOFileBlock::IOFileBlock(XrdOucCacheIO2 *io, XrdOucCacheStats &statsGlobal, Cache & cache)
   : IO(io, statsGlobal, cache)
{
   m_blocksize = Cache::GetInstance().RefConfiguration().m_hdfsbsize;
   GetBlockSizeFromPath();
}

//______________________________________________________________________________
XrdOucCacheIO* IOFileBlock::Detach()
{
   XrdOucCacheIO * io = GetInput();

   for (std::map<int, File*>::iterator it = m_blocks.begin(); it != m_blocks.end(); ++it)
   {
      m_statsGlobal.Add(it->second->GetStats());
   }

   m_cache.Detach(this);  // This will delete us!

   return io;
}

//______________________________________________________________________________
void IOFileBlock::GetBlockSizeFromPath()
{
    const static std::string tag = "hdfsbsize=";
    std::string path= GetInput()->Path();
    size_t pos1 = path.find(tag);
    size_t t = tag.length();
    if ( pos1 != path.npos)
    {
        pos1 += t;
        size_t pos2 = path.find("&", pos1 );
        if (pos2 != path.npos )
        {
            std::string bs = path.substr(pos1, pos2 - pos1);
            m_blocksize = atoi(bs.c_str());
        }
       else {
            m_blocksize = atoi(path.substr(pos1).c_str());
       }

        TRACEIO(Debug, "FileBlock::GetBlockSizeFromPath(), blocksize = " <<  m_blocksize );
    }
}

//______________________________________________________________________________
File* IOFileBlock::newBlockFile(long long off, int blocksize)
{
   XrdCl::URL url(GetInput()->Path());
   std::string fname = Cache::GetInstance().RefConfiguration().m_cache_dir + url.GetPath();

   std::stringstream ss;
   ss << fname;
   char offExt[64];
   // filename like <origpath>___<size>_<offset>
   sprintf(&offExt[0],"___%lld_%lld", m_blocksize, off );
   ss << &offExt[0];
   fname = ss.str();

   TRACEIO(Debug, "FileBlock::FileBlock(), create XrdFileCacheFile ");
   
   File* file;
   if (!(file = Cache::GetInstance().GetFileWithLocalPath(fname, this)))
   {
      file = new File(this, fname, off, blocksize);
      Cache::GetInstance().AddActive(this, file);
   }
      
   return file;
}

//______________________________________________________________________________
void IOFileBlock::RelinquishFile(File* f)
{
   // called from Cache::Detach() or Cache::GetFileWithLocalPath()
   // the object is in process of dying
   
   XrdSysMutexHelper lock(&m_mutex);
   for (std::map<int, File*>::iterator it = m_blocks.begin(); it != m_blocks.end(); ++it)
   {
      if (it->second == f)
      {
         m_blocks.erase(it++);
         break;
      }
      else
      {
         ++it;
      }
   }
}

//______________________________________________________________________________
bool IOFileBlock::ioActive()
{
   XrdSysMutexHelper lock(&m_mutex);

   for (std::map<int, File*>::iterator it = m_blocks.begin(); it != m_blocks.end(); ++it) {
      if (it->second->ioActive())
         return true;
   }
  
   return false;
}

//______________________________________________________________________________
int IOFileBlock::Read (char *buff, long long off, int size)
{
   // protect from reads over the file size

   long long fileSize = GetInput()->FSize();

   if (off >= fileSize)
      return 0;
   if (off < 0)
   {
      errno = EINVAL;
      return -1;
   }
   if (off + size > fileSize)
      size = fileSize - off;

   long long off0 = off;
   int idx_first = off0/m_blocksize;
   int idx_last = (off0+size-1)/m_blocksize;
   int bytes_read = 0;
   TRACEIO(Dump, "IOFileBlock::Read() "<< off << "@" << size << " block range ["<< idx_first << ", " << idx_last << "]");

   for (int blockIdx = idx_first; blockIdx <= idx_last; ++blockIdx )
   {
      // locate block
      File* fb;
      m_mutex.Lock();
      std::map<int, File*>::iterator it = m_blocks.find(blockIdx);
      if ( it != m_blocks.end() )
      {
         fb = it->second;
      }
      else
      {
         size_t pbs = m_blocksize;
         // check if this is last block
         int lastIOFileBlock = (fileSize-1)/m_blocksize;
         if (blockIdx == lastIOFileBlock )
         {
            pbs =  fileSize - blockIdx*m_blocksize;
            // TRACEIO(Dump, "IOFileBlock::Read() last block, change output file size to " << pbs);
         }

         fb = newBlockFile(blockIdx*m_blocksize, pbs);
         m_blocks.insert(std::pair<int,File*>(blockIdx, (File*) fb));
      }
      m_mutex.UnLock();

      // edit size if read request is reaching more than a block
      int readBlockSize = size;
      if (idx_first != idx_last)
      {
         if (blockIdx == idx_first)
         {
            readBlockSize = (blockIdx + 1) *m_blocksize - off0;
            TRACEIO(Dump, "Read partially till the end of the block");
         }
         else if (blockIdx == idx_last)
         {
            readBlockSize = (off0+size) - blockIdx*m_blocksize;
            TRACEIO(Dump, "Read partially till the end of the block %s");
         }
         else
         {
            readBlockSize = m_blocksize;
         }
      }
      assert(readBlockSize);

      TRACEIO(Dump, "IOFileBlock::Read() block[ " << blockIdx << "] read-block-size[" << readBlockSize << "], offset[" << readBlockSize << "] off = " << off );

      long long min  = blockIdx*m_blocksize;
      if ( off < min) { assert(0); }
      assert(off+readBlockSize <= (min + m_blocksize));
      int retvalBlock = fb->Read(buff, off, readBlockSize);

      TRACEIO(Dump, "IOFileBlock::Read()  Block read returned " << retvalBlock);
      if (retvalBlock ==  readBlockSize )
      {
         bytes_read += retvalBlock;
         buff += retvalBlock;
         off += retvalBlock;
      }
      else if (retvalBlock > 0) {
          TRACEIO(Warning, "IOFileBlock::Read() incomplete read, missing bytes " << readBlockSize-retvalBlock);
         return bytes_read + retvalBlock;
      }
      else
      {
          TRACEIO(Error, "IOFileBlock::Read() read error, retval" << retvalBlock);
         return retvalBlock;
      }
   }

   return bytes_read;
}
