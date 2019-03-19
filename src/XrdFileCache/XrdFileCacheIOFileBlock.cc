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
#include <fcntl.h>

#include "XrdFileCacheIOFileBlock.hh"
#include "XrdFileCache.hh"
#include "XrdFileCacheStats.hh"
#include "XrdFileCacheTrace.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSfs/XrdSfsInterface.hh"

#include "XrdOuc/XrdOucEnv.hh"

using namespace XrdFileCache;

//______________________________________________________________________________
IOFileBlock::IOFileBlock(XrdOucCacheIO2 *io, XrdOucCacheStats &statsGlobal, Cache & cache) :
  IO(io, statsGlobal, cache), m_localStat(0), m_info(cache.GetTrace(), false), m_infoFile(0)
{
   m_blocksize = Cache::GetInstance().RefConfiguration().m_hdfsbsize;
   GetBlockSizeFromPath();
   initLocalStat();
}

//______________________________________________________________________________
IOFileBlock::~IOFileBlock()
{
   // called from Detach() if no sync is needed or
   // from Cache's sync thread

   TRACEIO(Debug, "deleting IOFileBlock");
}

//______________________________________________________________________________
XrdOucCacheIO* IOFileBlock::Detach()
{
   // Called from XrdPosixFile destructor

   TRACEIO(Info, "Detach IOFileBlock");

   CloseInfoFile();
   {
     XrdSysMutexHelper lock(&m_mutex);
     for (std::map<int, File*>::iterator it = m_blocks.begin(); it != m_blocks.end(); ++it)
     {
        if (it->second)
        {
           it->second->RequestSyncOfDetachStats();
           m_cache.ReleaseFile(it->second, this);
        }
     }
   }
   XrdOucCacheIO *io = GetInput();
   delete this;
   return io;
}


//______________________________________________________________________________
void IOFileBlock::CloseInfoFile()
{
   // write access statistics to info file and close it
   // detach time is needed for file purge
   if (m_infoFile)
   {
      if (m_info.GetFileSize() > 0)
      {
         // We do not maintain access statistics for individual blocks.
         Stats as;
         m_info.WriteIOStatDetach(as);
      }
      m_info.Write(m_infoFile);
      m_infoFile->Fsync();
      m_infoFile->Close();

      delete m_infoFile;
      m_infoFile = 0;
   }
}

//______________________________________________________________________________
void IOFileBlock::GetBlockSizeFromPath()
{
   const static std::string tag = "hdfsbsize=";

   std::string path = GetInput()->Path();
   size_t pos1      = path.find(tag);
   size_t t         = tag.length();

   if (pos1 != path.npos)
   {
      pos1 += t;
      size_t pos2 = path.find("&", pos1);
      if (pos2 != path.npos )
      {
         std::string bs = path.substr(pos1, pos2 - pos1);
         m_blocksize = atoi(bs.c_str());
      }
      else
      {
         m_blocksize = atoi(path.substr(pos1).c_str());
      }

      TRACEIO(Debug, "FileBlock::GetBlockSizeFromPath(), blocksize = " <<  m_blocksize );
   }
}

//______________________________________________________________________________
File* IOFileBlock::newBlockFile(long long off, int blocksize)
{
   // NOTE: Can return 0 if opening of a local file fails!

   XrdCl::URL url(GetInput()->Path());
   std::string fname = url.GetPath();

   std::stringstream ss;
   ss << fname;
   char offExt[64];
   // filename like <origpath>___<size>_<offset>
   sprintf(&offExt[0], "___%lld_%lld", m_blocksize, off);
   ss << &offExt[0];
   fname = ss.str();

   TRACEIO(Debug, "FileBlock::FileBlock(), create XrdFileCacheFile ");

   File* file = Cache::GetInstance().GetFile(fname, this, off, blocksize);
   return file;
}

//______________________________________________________________________________
int IOFileBlock::Fstat(struct stat &sbuff)
{
   // local stat is create in constructor. if file was on disk before
   // attach that the only way stat was not successful is becuse there
   // were info file read errors
   if ( ! m_localStat) return -1;

   memcpy(&sbuff, m_localStat, sizeof(struct stat));
   return 0;
}

//______________________________________________________________________________
long long IOFileBlock::FSize()
{
   if ( ! m_localStat) return -1;

   return m_localStat->st_size;
}

//______________________________________________________________________________
int IOFileBlock::initLocalStat()
{
   XrdCl::URL url(GetPath());
   std::string path = url.GetPath();
   path += ".cinfo";

   int res = -1;
   struct stat tmpStat;
   XrdOucEnv myEnv;

   // try to read from existing file
   if (m_cache.GetOss()->Stat(path.c_str(), &tmpStat) == XrdOssOK)
   {
      m_infoFile = m_cache.GetOss()->newFile(m_cache.RefConfiguration().m_username.c_str());
      if (m_infoFile->Open(path.c_str(), O_RDWR, 0600, myEnv) == XrdOssOK)
      {
         if (m_info.Read(m_infoFile, path))
         {
            tmpStat.st_size = m_info.GetFileSize();
            TRACEIO(Info, "IOFileBlock::initCachedStat successfuly read size from existing info file = " << tmpStat.st_size);
            res = 0;
         }
         else
         {
            // file exist but can't read it
            TRACEIO(Debug, "IOFileBlock::initCachedStat info file is not complete");
         }
      }
   }

   // if there is no local info file, try to read from client and then save stat into a new *cinfo file
   if (res)
   {
      if (m_infoFile) { delete m_infoFile; m_infoFile = 0; }

      res = GetInput()->Fstat(tmpStat);
      TRACEIO(Debug, "IOFileBlock::initCachedStat get stat from client res= " << res << "size = " << tmpStat.st_size);
      if (res == 0)
      {
         if (m_cache.GetOss()->Create(m_cache.RefConfiguration().m_username.c_str(), path.c_str(), 0600, myEnv, XRDOSS_mkpath) ==  XrdOssOK)
         {
            m_infoFile = m_cache.GetOss()->newFile(m_cache.RefConfiguration().m_username.c_str());
            if (m_infoFile->Open(path.c_str(), O_RDWR, 0600, myEnv) == XrdOssOK)
            {
               // This is writing the top-level cinfo
               // The info file is used to get file size on defer open
               // don't initalize buffer, it does not hold useful information in this case
               m_info.SetBufferSize(m_cache.RefConfiguration().m_bufferSize);
               m_info.DisableDownloadStatus();
               m_info.SetFileSize(tmpStat.st_size);
               m_info.Write(m_infoFile, path);
               m_infoFile->Fsync();
            }
            else
            {
               TRACEIO(Error, "IOFileBlock::initCachedStat can't open info file path");
            }
         }
         else
         {
            TRACEIO(Error, "IOFileBlock::initCachedStat can't create info file path");
         }
      }
   }

   if (res == 0)
   {
      m_localStat = new struct stat;
      memcpy(m_localStat, &tmpStat, sizeof(struct stat));
   }

   return res;
}

//______________________________________________________________________________
bool IOFileBlock::ioActive()
{
   XrdSysMutexHelper lock(&m_mutex);

   bool active = false;

   for (std::map<int, File*>::iterator it = m_blocks.begin(); it != m_blocks.end(); ++it)
   {
      // Need to initiate stop on all File / block objects.
      if (it->second && it->second->ioActive(this))
      {
         active = true;
      }
   }

   return active;
}

//______________________________________________________________________________
int IOFileBlock::Read(char *buff, long long off, int size)
{
   // protect from reads over the file size

   long long fileSize = FSize();

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
   int idx_first  = off0 / m_blocksize;
   int idx_last   = (off0 + size - 1) / m_blocksize;
   int bytes_read = 0;
   TRACEIO(Dump, "IOFileBlock::Read() "<< off << "@" << size << " block range ["<< idx_first << ", " << idx_last << "]");

   for (int blockIdx = idx_first; blockIdx <= idx_last; ++blockIdx)
   {
      // locate block
      File *fb;
      m_mutex.Lock();
      std::map<int, File*>::iterator it = m_blocks.find(blockIdx);
      if (it != m_blocks.end())
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
            pbs = fileSize - blockIdx*m_blocksize;
            // TRACEIO(Dump, "IOFileBlock::Read() last block, change output file size to " << pbs);
         }

         // Note: File* can be 0 and stored as 0 if local open fails!
         fb = newBlockFile(blockIdx*m_blocksize, pbs);
         m_blocks.insert(std::make_pair(blockIdx, fb));
      }
      m_mutex.UnLock();

      // edit size if read request is reaching more than a block
      int readBlockSize = size;
      if (idx_first != idx_last)
      {
         if (blockIdx == idx_first)
         {
            readBlockSize = (blockIdx + 1) * m_blocksize - off0;
            TRACEIO(Dump, "Read partially till the end of the block");
         }
         else if (blockIdx == idx_last)
         {
            readBlockSize = (off0 + size) - blockIdx * m_blocksize;
            TRACEIO(Dump, "Read partially till the end of the block %s");
         }
         else
         {
            readBlockSize = m_blocksize;
         }
      }

      TRACEIO(Dump, "IOFileBlock::Read() block[ " << blockIdx << "] read-block-size[" << readBlockSize << "], offset[" << readBlockSize << "] off = " << off );

      int retvalBlock = (fb != 0) ?
         fb->Read(this, buff, off, readBlockSize) :
         GetInput()->Read(buff, off, readBlockSize);

      TRACEIO(Dump, "IOFileBlock::Read()  Block read returned " << retvalBlock);
      if (retvalBlock == readBlockSize)
      {
         bytes_read += retvalBlock;
         buff       += retvalBlock;
         off        += retvalBlock;
      }
      else if (retvalBlock > 0)
      {
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
