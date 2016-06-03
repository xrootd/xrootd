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

#include <sys/file.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucSxeq.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdFileCacheInfo.hh"
#include "XrdFileCache.hh"
#include "XrdFileCacheStats.hh"
#include "XrdFileCacheTrace.hh"

const char* XrdFileCache::Info::m_infoExtension = ".cinfo";
const char* XrdFileCache::Info::m_traceID = "Cinfo";

#define BIT(n)       (1ULL << (n))
using namespace XrdFileCache;


Info::Info(XrdOucTrace* trace, bool prefetchBuffer) :
   m_trace(trace),
   m_version(1),
   m_bufferSize(-1),
   m_hasPrefetchBuffer(prefetchBuffer),
   m_sizeInBits(0),
   m_buff_fetched(0), m_buff_write_called(0), m_buff_prefetch(0),
   m_accessCnt(0),
   m_complete(false)
{
}

Info::~Info()
{
   if (m_buff_fetched) free(m_buff_fetched);
   if (m_buff_write_called) free(m_buff_write_called);
   if (m_buff_prefetch) free(m_buff_prefetch);
}

//______________________________________________________________________________
void Info::SetBufferSize(long long bs)
{
   // Needed only info is created first time in File::Open()
   m_bufferSize = bs;
}

//______________________________________________________________________________
void Info::SetFileSize(long long fs)
{
   m_fileSize = fs;
   ResizeBits((m_fileSize-1)/m_bufferSize + 1) ;
}

//______________________________________________________________________________


void Info::ResizeBits(int s)
{
   m_sizeInBits = s;
   m_buff_fetched = (unsigned char*)malloc(GetSizeInBytes());
   m_buff_write_called = (unsigned char*)malloc(GetSizeInBytes());
   memset(m_buff_fetched, 0, GetSizeInBytes());
   memset(m_buff_write_called, 0, GetSizeInBytes());
   if (m_hasPrefetchBuffer) {
      m_buff_prefetch = (unsigned char*)malloc(GetSizeInBytes());
      memset(m_buff_prefetch, 0, GetSizeInBytes());
   }
}

//______________________________________________________________________________


int Info::Read(XrdOssDF* fp)
{
   // does not need lock, called only in File::Open
   // before File::Run() starts

   int off = 0;
   int version;
   off += fp->Read(&version, off, sizeof(int));
   if (version != m_version) {
      TRACE(Error, "Info:::Read(), incomatible file version");
       return 0;
   }

   off += fp->Read(&m_bufferSize, off, sizeof(long long));
   if (off <= 0) return off;

   long long fs;
   off += fp->Read(&fs, off, sizeof(long long));
   SetFileSize(fs);

   off += fp->Read(m_buff_fetched, off, GetSizeInBytes());
   assert (off == GetHeaderSize());

   memcpy(m_buff_write_called, m_buff_fetched, GetSizeInBytes());
   m_complete = IsAnythingEmptyInRng(0, m_sizeInBits - 1) ? false : true;


   off += fp->Read(&m_accessCnt, off, sizeof(int));
   TRACE(Dump, "Info:::Read() complete "<< m_complete << " access_cnt " << m_accessCnt);


   if (m_hasPrefetchBuffer) {
      m_buff_prefetch = (unsigned char*)malloc(GetSizeInBytes());
      memset(m_buff_prefetch, 0, GetSizeInBytes());
   }

   return off;
}

//______________________________________________________________________________


int Info::GetHeaderSize() const
{
   // version + buffersize + file-size + download-status-array
   return sizeof(int) + sizeof(long long) + sizeof(long long) + GetSizeInBytes();
}

//______________________________________________________________________________
void Info::WriteHeader(XrdOssDF* fp)
{
   int flr = XrdOucSxeq::Serialize(fp->getFD(), XrdOucSxeq::noWait);
   if (flr) TRACE(Error, "Info::WriteHeader() lock failed " << strerror(errno));

   long long off = 0;
   off += fp->Write(&m_version, off, sizeof(int));
   off += fp->Write(&m_bufferSize, off, sizeof(long long));

   off += fp->Write(&m_fileSize, off, sizeof(long long));
   off += fp->Write(m_buff_write_called, off, GetSizeInBytes());

   flr = XrdOucSxeq::Release(fp->getFD());
   if (flr) TRACE(Error, "Info::WriteHeader() un-lock failed");

   assert (off == GetHeaderSize());
}

//______________________________________________________________________________
void Info::AppendIOStat(AStat& as, XrdOssDF* fp)
{
   TRACE(Dump, "Info:::AppendIOStat()");

   int flr = XrdOucSxeq::Serialize(fp->getFD(), 0);
   if (flr) {
      TRACE(Error, "Info::AppendIOStat() lock failed");
      return;
   }

   m_accessCnt++;
   long long off = GetHeaderSize();
   off += fp->Write(&m_accessCnt, off, sizeof(int));
   off += (m_accessCnt-1)*sizeof(AStat);
 
   long long ws = fp->Write(&as, off, sizeof(AStat));
   flr = XrdOucSxeq::Release(fp->getFD());
   if (flr) {
      TRACE(Error, "Info::AppenIOStat() un-lock failed");
      return;
   }

   if ( ws != sizeof(AStat)) { assert(0); }
}

//______________________________________________________________________________
bool Info::GetLatestDetachTime(time_t& t, XrdOssDF* fp) const
{
   bool res = false;

   int flr = XrdOucSxeq::Serialize(fp->getFD(), XrdOucSxeq::Share);
   if (flr) {
       TRACE(Error, "Info::GetLatestAttachTime() lock failed");
       return false;
   }
   if (m_accessCnt)
   {
      AStat     stat;
      long long off      = GetHeaderSize() + sizeof(int) + (m_accessCnt-1)*sizeof(AStat);
      ssize_t   read_res = fp->Read(&stat, off, sizeof(AStat));
      if (read_res == sizeof(AStat))
      {
         t = stat.DetachTime;
         res = true;
      }
      else
      {
         TRACE(Error, " Info::GetLatestAttachTime() can't get latest access stat ");
         return false;
      }
   }


   flr = XrdOucSxeq::Release(fp->getFD());
   if (flr) TRACE(Error, "Info::GetLatestAttachTime() lock failed");

   return res;
}
