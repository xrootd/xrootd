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

#include "XrdPfcIOFile.hh"
#include "XrdPfcStats.hh"
#include "XrdPfcTrace.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"

using namespace XrdPfc;

//______________________________________________________________________________
IOFile::IOFile(XrdOucCacheIO *io, Cache & cache) :
   IO(io, cache),
   m_file(0),
   m_localStat(0)
{
   m_file = Cache::GetInstance().GetFile(GetFilename(), this);
}

//______________________________________________________________________________
IOFile::~IOFile()
{
   // called from Detach() if no sync is needed or
   // from Cache's sync thread
   TRACEIO(Debug, "~IOFile() " << this);

   delete m_localStat;
}

//______________________________________________________________________________
int IOFile::Fstat(struct stat &sbuff)
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
long long IOFile::FSize()
{
   return m_file->GetFileSize();
}

//______________________________________________________________________________
int IOFile::initCachedStat(const char* path)
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
void IOFile::Update(XrdOucCacheIO &iocp)
{
   IO::Update(iocp);
   m_file->ioUpdated(this);
}

//______________________________________________________________________________
bool IOFile::ioActive()
{
   RefreshLocation();
   return m_file->ioActive(this);
}

//______________________________________________________________________________
void IOFile::DetachFinalize()
{
   // Effectively a destructor.

   TRACE(Info, "DetachFinalize() " << this);

   m_file->RequestSyncOfDetachStats();
   Cache::GetInstance().ReleaseFile(m_file, this);

   delete this;
}


//==============================================================================
// Read and pgRead - sync / async and helpers
//==============================================================================

//______________________________________________________________________________
int IOFile::Read(char *buff, long long off, int size)
{
   TRACEIO(Debug, "Read() sync "<< this << " off: " << off << " size: " << size);

   int retval;
   XrdSysCondVar cond;

   auto end_foo = [&](int result) {
      cond.Lock();
      retval = result;
      cond.Signal();
      cond.UnLock();
   };

   cond.Lock();
   retval = ReadBegin(buff, off, size, end_foo);
   if (retval == -EWOULDBLOCK)
   {
      cond.Wait();
   }
   cond.UnLock();
   return ReadEnd(retval, nullptr);
}

//______________________________________________________________________________
void IOFile::Read(XrdOucCacheIOCB &iocb, char *buff, long long off, int size)
{
   TRACEIO(Debug, "Read() async "<< this << " off: " << off << " size: " << size);

   auto end_foo = [=, &iocb](int result) {
      this->ReadEnd(result, &iocb);
   };

   int retval = ReadBegin(buff, off, size, end_foo);
   if (retval != -EWOULDBLOCK)
   {
      end_foo(retval);
   }
}

//______________________________________________________________________________
void IOFile::pgRead(XrdOucCacheIOCB &iocb, char *buff, long long off, int size,
                    std::vector<uint32_t> &csvec, uint64_t opts, int *csfix)
{
   TRACEIO(Debug, "pgRead() async "<< this << " off: " << off << " size: " << size);

   auto end_foo = [=, &iocb, &csvec](int result) {
      if (result > 0 && (opts & XrdOucCacheIO::forceCS))
         XrdOucPgrwUtils::csCalc((const char *)buff, (ssize_t)off, (size_t)result, csvec);
      this->ReadEnd(result, &iocb);
   };

   int retval = ReadBegin(buff, off, size, end_foo);
   if (retval != -EWOULDBLOCK)
   {
      end_foo(retval);
   }
}

//______________________________________________________________________________
int IOFile::ReadBegin(char *buff, long long off, int size, ReadReqComplete_foo rrc_func)
{
   // protect from reads over the file size
   if (off >= FSize()) {
      size = 0;
      return 0;
   }
   if (off < 0) {
      return -EINVAL;
   }
   if (off + size > FSize()) {
      size = FSize() - off;
   }

   return m_file->Read(this, buff, off, size, rrc_func);
}

//______________________________________________________________________________
int IOFile::ReadEnd(int retval, XrdOucCacheIOCB *iocb)
{
   TRACEIO(Debug, "ReadEnd() " << (iocb ? "a" : "") << "sync " << this << " retval: " << retval);

   if (retval < 0)
   {
      TRACEIO(Warning, "ReadEnd() error in File::Read(), exit status=" << retval
              << ", error=" << XrdSysE2T(-retval));
   }

   if (iocb)
   {
      iocb->Done(retval);
   }

   return retval;
}


//==============================================================================
// ReadV
//==============================================================================

//______________________________________________________________________________
int IOFile::ReadV(const XrdOucIOVec *readV, int n)
{
   TRACEIO(Dump, "ReadV() sync, get " <<  n << " chunks" );

   int retval;
   XrdSysCondVar cond;

   auto end_foo = [&](int result) {
      cond.Lock();
      retval = result;
      cond.Signal();
      cond.UnLock();
   };

   cond.Lock();
   retval = ReadVBegin(readV, n, end_foo);
   if (retval == -EWOULDBLOCK)
   {
      cond.Wait();
   }
   cond.UnLock();
   return ReadVEnd(retval, n, nullptr);
}

//______________________________________________________________________________
void IOFile::ReadV(XrdOucCacheIOCB &iocb, const XrdOucIOVec *readV, int n)
{
   TRACEIO(Dump, "ReadV() async, n_chunks: " <<  n);

   auto end_foo = [=, &iocb](int result) {
      this->ReadVEnd(result, n, &iocb);
   };

   int retval = ReadVBegin(readV, n, end_foo);
   if (retval != -EWOULDBLOCK)
   {
      end_foo(retval);
   }
}

//______________________________________________________________________________
int IOFile::ReadVBegin(const XrdOucIOVec *readV, int n, ReadReqComplete_foo rrc_func)
{
   long long file_size = FSize();
   for (int i = 0; i < n; ++i)
   {
      const XrdOucIOVec &vr = readV[i];
      if (vr.offset < 0 || vr.offset >= file_size ||
          vr.offset + vr.size > file_size)
      {
         return -EINVAL;
      }
   }

   return m_file->ReadV(this, readV, n, rrc_func);
}

//______________________________________________________________________________
int IOFile::ReadVEnd(int retval, int n, XrdOucCacheIOCB *iocb)
{
   TRACEIO(Debug, "ReadVEnd() " << (iocb ? "a" : "") << "sync " << this << " retval: " << retval << " n_chunks: " << n);

   if (retval < 0)
   {
      TRACEIO(Warning, "ReadVEnd() error in File::ReadV(), exit status=" << retval
              << ", error=" << XrdSysE2T(-retval));
   }

   if (iocb)
   {
      iocb->Done(retval);
   }

   return retval;
}