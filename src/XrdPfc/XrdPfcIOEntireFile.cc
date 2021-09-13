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

#include <cstdio>
#include <fcntl.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "XrdPfcIOEntireFile.hh"
#include "XrdPfcStats.hh"
#include "XrdPfcTrace.hh"

#include "XrdOuc/XrdOucEnv.hh"

using namespace XrdPfc;

//______________________________________________________________________________
IOEntireFile::IOEntireFile(XrdOucCacheIO *io, Cache & cache) :
   IO(io, cache),
   m_file(0),
   m_localStat(0)
{
   m_file = Cache::GetInstance().GetFile(GetFilename(), this);
}

//______________________________________________________________________________
IOEntireFile::~IOEntireFile()
{
   // called from Detach() if no sync is needed or
   // from Cache's sync thread
   TRACEIO(Debug, "~IOEntireFile() " << this);

   delete m_localStat;
}

//______________________________________________________________________________
int IOEntireFile::Fstat(struct stat &sbuff)
{
   std::string name = GetFilename() + Info::s_infoExtension;

   int res = 0;
   if( ! m_localStat)
   {
      res = initCachedStat(name.c_str());
      if (res) return res;
   }

   memcpy(&sbuff, m_localStat, sizeof(struct stat));
   return 0;
}

//______________________________________________________________________________
long long IOEntireFile::FSize()
{
   return m_file->GetFileSize();
}

//______________________________________________________________________________
int IOEntireFile::initCachedStat(const char* path)
{
   // Called indirectly from the constructor.

   static const char* trace_pfx = "initCachedStat ";

   int res = -1;
   struct stat tmpStat;

   if (m_cache.GetOss()->Stat(path, &tmpStat) == XrdOssOK)
   {
      XrdOssDF* infoFile = m_cache.GetOss()->newFile(Cache::GetInstance().RefConfiguration().m_username.c_str());
      XrdOucEnv myEnv;
      int       res_open;
      if ((res_open = infoFile->Open(path, O_RDONLY, 0600, myEnv)) == XrdOssOK)
      {
         Info info(m_cache.GetTrace());
         if (info.Read(infoFile, path))
         {
            tmpStat.st_size = info.GetFileSize();
            TRACEIO(Info, trace_pfx << "successfully read size from info file = " << tmpStat.st_size);
            res = 0;
         }
         else
         {
            // file exist but can't read it
            TRACEIO(Info, trace_pfx << "info file is incomplete or corrupt");
         }
      }
      else
      {
         TRACEIO(Error, trace_pfx << "can't open info file " << XrdSysE2T(-res_open));
      }
      infoFile->Close();
      delete infoFile;
   }

   if (res)
   {
      res = GetInput()->Fstat(tmpStat);
      TRACEIO(Debug, trace_pfx << "got stat from client res = " << res << ", size = " << tmpStat.st_size);
   }

   if (res == 0)
   {
      m_localStat = new struct stat;
      memcpy(m_localStat, &tmpStat, sizeof(struct stat));
   }
   return res;
}

//______________________________________________________________________________
void IOEntireFile::Update(XrdOucCacheIO &iocp)
{
   IO::Update(iocp);
   m_file->ioUpdated(this);
}

//______________________________________________________________________________
bool IOEntireFile::ioActive()
{
   RefreshLocation();
   return m_file->ioActive(this);
}

//______________________________________________________________________________
void IOEntireFile::DetachFinalize()
{
   // Effectively a destructor.

   TRACE(Info, "DetachFinalize() " << this);

   m_file->RequestSyncOfDetachStats();
   Cache::GetInstance().ReleaseFile(m_file, this);

   delete this;
}

//______________________________________________________________________________
int IOEntireFile::Read(char *buff, long long off, int size)
{
   TRACEIO(Dump, "Read() "<< this << " off: " << off << " size: " << size);

   // protect from reads over the file size
   if (off >= FSize())
      return 0;
   if (off < 0)
   {
      return -EINVAL;
   }
   if (off + size > FSize())
      size = FSize() - off;


   ssize_t bytes_read = 0;
   ssize_t retval = 0;

   retval = m_file->Read(this, buff, off, size);
   if (retval >= 0)
   {
      bytes_read  = retval;
      size       -= retval;

      // All errors like this should have been already captured by File::Read()
      // and reflected in its retval.
      if (size > 0)
         TRACEIO(Warning, "Read() bytes missed " << size);
   }
   else
   {
      TRACEIO(Warning, "Read() error in File::Read(), exit status=" << retval
              << ", error=" << XrdSysE2T(-retval));
   }

   return (retval < 0) ? retval : bytes_read;
}


/*
 * Perform a readv from the cache
 */
int IOEntireFile::ReadV(const XrdOucIOVec *readV, int n)
{
   TRACEIO(Dump, "ReadV(), get " <<  n << " requests" );
   return m_file->ReadV(this, readV, n);
}
