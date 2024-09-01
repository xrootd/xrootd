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

#include "XrdPfcIOFile.hh"
#include "XrdPfcStats.hh"
#include "XrdPfcTrace.hh"

#include "XrdOss/XrdOss.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"

#include <cstdio>
#include <fcntl.h>

using namespace XrdPfc;

//______________________________________________________________________________
IOFile::IOFile(XrdOucCacheIO *io, Cache & cache) :
   IO(io, cache),
   m_file(0)
{
   m_file = Cache::GetInstance().GetFile(GetFilename(), this);
}

//______________________________________________________________________________
IOFile::~IOFile()
{
   // called from Detach() if no sync is needed or
   // from Cache's sync thread
   TRACEIO(Debug, "~IOFile() " << this);
}

//______________________________________________________________________________
int IOFile::Fstat(struct stat &sbuff)
{
   // This is only called during Cache::Attach / Cache::GetFile() for file creation.
   // Should really be separate name but one needs to change virtual interface
   // so initialStat() becomes virtual in the base-class.
   // Also, IOFileBlock should be ditched.
   if ( ! m_file) {
      return initialStat(sbuff);
   }

   return m_file->Fstat(sbuff);
}

//______________________________________________________________________________
long long IOFile::FSize()
{
   return m_file->GetFileSize();
}

//______________________________________________________________________________
int IOFile::initialStat(struct stat &sbuff)
{
   // Get stat to determine file-size.
   // Called indirectly from the constructor, via Cache::GetFile().
   // Either read it from cinfo file or obtain it from the remote IO.

   static const char* trace_pfx = "initialStat ";

   std::string fname = GetFilename();
   if (m_cache.GetOss()->Stat(fname.c_str(), &sbuff) == XrdOssOK)
   {
      long long file_size = m_cache.DetermineFullFileSize(fname + Info::s_infoExtension);
      if (file_size >= 0)
      {
         sbuff.st_size = file_size;
         TRACEIO(Info, trace_pfx << "successfully read size " << sbuff.st_size << " from info file");
         return 0;
      }
      TRACEIO(Error, trace_pfx << "failed reading from info file " << XrdSysE2T(-file_size));
   }

   int res = GetInput()->Fstat(sbuff);
   TRACEIO(Debug, trace_pfx << "stat from client res = " << res << ", size = " << sbuff.st_size);

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

   TRACE(Debug, "DetachFinalize() " << this);

   m_file->RequestSyncOfDetachStats();
   Cache::GetInstance().ReleaseFile(m_file, this);

   if (( ! m_error_counts.empty() || m_incomplete_count > 0) && XRD_TRACE What >= TRACE_Error) {
      char info[1024];
      int pos = 0, cap = 1024;
      bool truncated = false;
      for (auto [err, cnt] : m_error_counts) {
         int len = snprintf(&info[pos], cap, " ( %d : %d)", err, cnt);
         if (len >= cap) {
            truncated = true;
            break;
         }
         pos += len;
         cap -= len;
      }
      TRACE(Error, "DetachFinalize() " << this << " n_incomplete_reads=" << m_incomplete_count
            << ", block (error : count) report:" << info << (truncated ? " - truncated" : ""));
   }

   delete this;
}


//==============================================================================
// Read and pgRead - sync / async and helpers
//==============================================================================

//______________________________________________________________________________
int IOFile::Read(char *buff, long long off, int size)
{
   ++m_active_read_reqs;

   auto *rh = new ReadReqRHCond(ObtainReadSid(), nullptr);

   TRACEIO(Dump, "Read() sync " << this << " sid: " << Xrd::hex1 << rh->m_seq_id << " off: " << off << " size: " << size);

   rh->m_cond.Lock();
   int retval = ReadBegin(buff, off, size, rh);
   if (retval == -EWOULDBLOCK)
   {
      rh->m_cond.Wait();
      retval = rh->m_retval;
   }
   rh->m_cond.UnLock();

   return ReadEnd(retval, rh);
}

//______________________________________________________________________________
void IOFile::Read(XrdOucCacheIOCB &iocb, char *buff, long long off, int size)
{
   struct ZHandler : ReadReqRH
   {  using ReadReqRH::ReadReqRH;
      IOFile *m_io = nullptr;

      void Done(int result) override {
         m_io->ReadEnd(result, this);
      }
   };

   ++m_active_read_reqs;

   auto *rh = new ZHandler(ObtainReadSid(), &iocb);
   rh->m_io = this;

   TRACEIO(Dump, "Read() async " << this << " sid: " << Xrd::hex1 << rh->m_seq_id << " off: " << off << " size: " << size);

   int retval = ReadBegin(buff, off, size, rh);
   if (retval != -EWOULDBLOCK)
   {
      rh->Done(retval);
   }
}

//______________________________________________________________________________
void IOFile::pgRead(XrdOucCacheIOCB &iocb, char *buff, long long off, int size,
                    std::vector<uint32_t> &csvec, uint64_t opts, int *csfix)
{
   struct ZHandler : ReadReqRH
   {  using ReadReqRH::ReadReqRH;
      IOFile *m_io = nullptr;
      std::function<void (int)> m_lambda {0};

      void Done(int result) override {
         if (m_lambda) m_lambda(result);
         m_io->ReadEnd(result, this);
      }
   };

   ++m_active_read_reqs;

   auto *rh = new ZHandler(ObtainReadSid(), &iocb);
   rh->m_io = this;

   TRACEIO(Dump, "pgRead() async " << this << " sid: " << Xrd::hex1 << rh->m_seq_id << " off: " << off << " size: " << size);

   if (opts & XrdOucCacheIO::forceCS)
      rh->m_lambda = [=, &csvec](int result) {
         if (result > 0)
            XrdOucPgrwUtils::csCalc((const char *)buff, (ssize_t)off, (size_t)result, csvec);
      };

   int retval = ReadBegin(buff, off, size, rh);
   if (retval != -EWOULDBLOCK)
   {
      rh->Done(retval);
   }
}

//______________________________________________________________________________
int IOFile::ReadBegin(char *buff, long long off, int size, ReadReqRH *rh)
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
   rh->m_expected_size = size;

   return m_file->Read(this, buff, off, size, rh);
}

//______________________________________________________________________________
int IOFile::ReadEnd(int retval, ReadReqRH *rh)
{
   TRACEIO(Dump, "ReadEnd() " << (rh->m_iocb ? "a" : "") << "sync " << this << " sid: " << Xrd::hex1 << rh->m_seq_id << " retval: " << retval << " expected_size: " << rh->m_expected_size);

   if (retval < 0) {
      TRACEIO(Debug, "ReadEnd() error in File::Read(), exit status=" << retval << ", error=" << XrdSysE2T(-retval) << " sid: " << Xrd::hex1 << rh->m_seq_id);
   } else if (retval < rh->m_expected_size) {
      TRACEIO(Debug, "ReadEnd() bytes missed " << rh->m_expected_size - retval << " sid: " << Xrd::hex1 << rh->m_seq_id);
   }
   if (rh->m_iocb)
      rh->m_iocb->Done(retval);

   delete rh;

   --m_active_read_reqs;

   return retval;
}


//==============================================================================
// ReadV
//==============================================================================

//______________________________________________________________________________
int IOFile::ReadV(const XrdOucIOVec *readV, int n)
{
   ++m_active_read_reqs;

   auto *rh = new ReadReqRHCond(ObtainReadSid(), nullptr);

   TRACEIO(Dump, "ReadV() sync " << this << " sid: " << Xrd::hex1 << rh->m_seq_id << " n_chunks: " <<  n);

   rh->m_cond.Lock();
   int retval = ReadVBegin(readV, n, rh);
   if (retval == -EWOULDBLOCK)
   {
      rh->m_cond.Wait();
      retval = rh->m_retval;
   }
   rh->m_cond.UnLock();
   return ReadVEnd(retval, rh);
}

//______________________________________________________________________________
void IOFile::ReadV(XrdOucCacheIOCB &iocb, const XrdOucIOVec *readV, int n)
{
   struct ZHandler : ReadReqRH
   {  using ReadReqRH::ReadReqRH;
      IOFile *m_io = nullptr;

      void Done(int result) override { m_io-> ReadVEnd(result, this); }
   };

   ++m_active_read_reqs;

   auto *rh = new ZHandler(ObtainReadSid(), &iocb);
   rh->m_io = this;

   TRACEIO(Dump, "ReadV() async " << this << " sid: " << Xrd::hex1 << rh->m_seq_id << " n_chunks: " <<  n);

   int retval = ReadVBegin(readV, n, rh);
   if (retval != -EWOULDBLOCK)
   {
      rh->Done(retval);
   }
}

//______________________________________________________________________________
int IOFile::ReadVBegin(const XrdOucIOVec *readV, int n, ReadReqRH *rh)
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
      rh->m_expected_size += vr.size;
   }
   rh->m_n_chunks = n;

   return m_file->ReadV(this, readV, n, rh);
}

//______________________________________________________________________________
int IOFile::ReadVEnd(int retval, ReadReqRH *rh)
{
   TRACEIO(Dump, "ReadVEnd() " << (rh->m_iocb ? "a" : "") << "sync " << this << " sid: " << Xrd::hex1 << rh->m_seq_id <<
                 " retval: " << retval << " n_chunks: " << rh->m_n_chunks << " expected_size: " << rh->m_expected_size);

   if (retval < 0) {
      TRACEIO(Debug, "ReadVEnd() error in File::ReadV(), exit status=" << retval << ", error=" << XrdSysE2T(-retval));
   } else if (retval < rh->m_expected_size) {
      TRACEIO(Debug, "ReadVEnd() bytes missed " << rh->m_expected_size - retval);
   }
   if (rh->m_iocb)
      rh->m_iocb->Done(retval);

   delete rh;

   --m_active_read_reqs;

   return retval;
}
