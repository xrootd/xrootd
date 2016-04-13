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
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdFileCacheInfo.hh"
#include "XrdFileCache.hh"

const char* XrdFileCache::Info::m_infoExtension = ".cinfo";

#define BIT(n)       (1ULL << (n))
using namespace XrdFileCache;


Info::Info(long long iBufferSize) :
   m_version(0),
   m_bufferSize(iBufferSize),
   m_sizeInBits(0), m_buff_fetched(0), m_buff_write_called(0),
   m_accessCnt(0),
   m_complete(false)
{
}

Info::~Info()
{
   if (m_buff_fetched) free(m_buff_fetched);
   if (m_buff_write_called) free(m_buff_write_called);
}

//______________________________________________________________________________


void Info::ResizeBits(int s)
{
   m_sizeInBits = s;
   m_buff_fetched = (unsigned char*)malloc(GetSizeInBytes());
   m_buff_write_called = (unsigned char*)malloc(GetSizeInBytes());
   memset(m_buff_fetched, 0, GetSizeInBytes());
   memset(m_buff_write_called, 0, GetSizeInBytes());
}

//______________________________________________________________________________


int Info::Read(XrdOssDF* fp)
{
   // does not need lock, called only in Prefetch::Open
   // before Prefetch::Run() starts

   int off = 0;
   off += fp->Read(&m_version, off, sizeof(int));
   off += fp->Read(&m_bufferSize, off, sizeof(long long));
   if (off <= 0) return off;

   int sb;
   off += fp->Read(&sb, off, sizeof(int));
   ResizeBits(sb);

   off += fp->Read(m_buff_fetched, off, GetSizeInBytes());
   assert (off == GetHeaderSize());

   memcpy(m_buff_write_called, m_buff_fetched, GetSizeInBytes());
   m_complete = IsAnythingEmptyInRng(0, sb-1) ? false : true;


   off += fp->Read(&m_accessCnt, off, sizeof(int));
   clLog()->Dump(XrdCl::AppMsg, "Info:::Read() complete %d access_cnt %d", m_complete, m_accessCnt);
   return off;
}

//______________________________________________________________________________


int Info::GetHeaderSize() const
{
   // version + buffersize + download-status-array-size + download-status-array
   return sizeof(int) + sizeof(long long) + sizeof(int) + GetSizeInBytes();
}

//______________________________________________________________________________
void Info::WriteHeader(XrdOssDF* fp)
{
   int flr = XrdOucSxeq::Serialize(fp->getFD(), XrdOucSxeq::noWait);
   if (flr) clLog()->Error(XrdCl::AppMsg, "WriteHeader() lock failed %s \n", strerror(errno));

   long long off = 0;
   off += fp->Write(&m_version, off, sizeof(int));
   off += fp->Write(&m_bufferSize, off, sizeof(long long));

   int nb = GetSizeInBits();
   off += fp->Write(&nb, off, sizeof(int));
   off += fp->Write(m_buff_write_called, off, GetSizeInBytes());

   flr = XrdOucSxeq::Release(fp->getFD());
   if (flr) clLog()->Error(XrdCl::AppMsg, "WriteHeader() un-lock failed \n");

   assert (off == GetHeaderSize());
}

//______________________________________________________________________________
void Info::AppendIOStat(AStat& as, XrdOssDF* fp)
{
   clLog()->Info(XrdCl::AppMsg, "Info:::AppendIOStat()");

   int flr = XrdOucSxeq::Serialize(fp->getFD(), 0);
   if (flr) clLog()->Error(XrdCl::AppMsg, "AppendIOStat() lock failed \n");

   m_accessCnt++;
   long long off = GetHeaderSize();
   off += fp->Write(&m_accessCnt, off, sizeof(int));
   off += (m_accessCnt-1)*sizeof(AStat);
 
   long long ws = fp->Write(&as, off, sizeof(AStat));
   flr = XrdOucSxeq::Release(fp->getFD());
   if (flr) clLog()->Error(XrdCl::AppMsg, "AppenIOStat() un-lock failed \n");

   if ( ws != sizeof(AStat)) { assert(0); }
}

//______________________________________________________________________________
bool Info::GetLatestDetachTime(time_t& t, XrdOssDF* fp) const
{
   bool res = false;

   int flr = XrdOucSxeq::Serialize(fp->getFD(), XrdOucSxeq::Share);
   if (flr) clLog()->Error(XrdCl::AppMsg, "Info::GetLatestAttachTime() lock failed \n");
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
         clLog()->Error(XrdCl::AppMsg, " Info::GetLatestAttachTime() can't get latest access stat. read bytes = %d", res);
      }
   }


   flr = XrdOucSxeq::Release(fp->getFD());
   if (flr) clLog()->Error(XrdCl::AppMsg, "Info::GetLatestAttachTime() lock failed \n");

   return res;
}
